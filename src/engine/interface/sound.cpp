// sound.cpp: basic positional sound using sdl_mixer

#include "../libprimis-headers/cube.h"

#include "SDL_mixer.h"

#include "console.h"
#include "control.h"
#include "cs.h"
#include "input.h"
#include "menus.h"

#include "render/rendergl.h" //needed to get camera position

#include "world/entities.h"
#include "world/world.h"

bool nosound = true;

struct SoundSample
{
    char *name;
    Mix_Chunk *chunk;

    SoundSample() : name(nullptr), chunk(nullptr) {}
    ~SoundSample()
    {
        delete[] name;
        name = nullptr;
    }

    void cleanup()
    {
        if(chunk)
        {
            Mix_FreeChunk(chunk);
            chunk = nullptr;
        }
    }

    bool load(const char *dir);
};

struct soundslot
{
    SoundSample *sample;
    int volume;
};

struct SoundConfig
{
    int slots, numslots;
    int maxuses;

    bool hasslot(const soundslot *p, const std::vector<soundslot> &v) const
    {
        return p >= v.data() + slots && p < v.data() + slots+numslots && slots+numslots < static_cast<long>(v.size());
    }

    int chooseslot(int flags) const
    {
        if(flags&Music_NoAlt || numslots <= 1)
        {
            return slots;
        }
        if(flags&Music_UseAlt)
        {
            return slots + 1 + randomint(numslots - 1);
        }
        return slots + randomint(numslots);
    }
};

//sound channel object that is allocated to every sound emitter in use
//(entities, players, weapons, etc.)
//defined in world coordinates, and position mixing is done for the player dynamically
struct SoundChannel
{
    int id;
    bool inuse;
    vec loc;
    soundslot *slot;
    extentity *ent;
    int radius, volume, pan, flags;
    bool dirty;

    SoundChannel(int id) : id(id) { reset(); }

    bool hasloc() const
    {
        return loc.x >= -1e15f;
    }

    void clearloc()
    {
        loc = vec(-1e16f, -1e16f, -1e16f);
    }

    void reset()
    {
        inuse  = false;
        clearloc();
        slot   = nullptr;
        ent    = nullptr;
        radius = 0;
        volume = -1;
        pan    = -1;
        flags  = 0;
        dirty  = false;
    }
};

static std::vector<SoundChannel> channels;
int maxchannels = 0;

//creates a new SoundChannel object with passed properties
static SoundChannel &newchannel(int n, soundslot *slot, const vec *loc = nullptr, extentity *ent = nullptr, int flags = 0, int radius = 0)
{
    if(ent)
    {
        loc = &ent->o;
        ent->flags |= EntFlag_Sound;
    }
    while(!(static_cast<long>(channels.size()) > n))
    {
        channels.push_back(channels.size());
    }
    SoundChannel &chan = channels[n];
    chan.reset();
    chan.inuse = true;
    if(loc)
    {
        chan.loc = *loc;
    }
    chan.slot = slot;
    chan.ent = ent;
    chan.flags = 0;
    chan.radius = radius;
    return chan;
}

//sets a channel as not being in use
static void freechannel(int n)
{
    if(!(static_cast<long>(channels.size()) > n) || !channels[n].inuse)
    {
        return;
    }
    SoundChannel &chan = channels[n];
    chan.inuse = false;
    if(chan.ent)
    {
        chan.ent->flags &= ~EntFlag_Sound;
    }
}

static void syncchannel(SoundChannel &chan)
{
    if(!chan.dirty)
    {
        return;
    }
    if(!Mix_FadingChannel(chan.id))
    {
        Mix_Volume(chan.id, chan.volume);
    }
    Mix_SetPanning(chan.id, 255-chan.pan, chan.pan);
    chan.dirty = false;
}

static void stopchannels()
{
    for(uint i = 0; i < channels.size(); i++)
    {
        SoundChannel &chan = channels[i];
        if(!chan.inuse) //don't clear channels that are already flagged as unused
        {
            continue;
        }
        Mix_HaltChannel(i);
        freechannel(i);
    }
}

static void setmusicvol(int musicvol);

VARFP(soundvol, 0, 255, 255,
    if(!soundvol)
    { //don't use sound infrastructure if volume is 0
        stopchannels();
        setmusicvol(0);
    }
);

VARFP(musicvol, 0, 60, 255, setmusicvol(soundvol ? musicvol : 0)); //background music volume

static char *musicfile    = nullptr,
            *musicdonecmd = nullptr;

static Mix_Music *music    = nullptr;
static SDL_RWops *musicrw  = nullptr;
static stream *musicstream = nullptr;

static void setmusicvol(int musicvol)
{
    if(nosound) //don't modulate music that isn't there
    {
        return;
    }
    if(music)
    {
        Mix_VolumeMusic((musicvol*MIX_MAX_VOLUME)/255);
    }
}

static void stopmusic()
{
    if(nosound) //don't stop music that isn't there
    {
        return;
    }
    delete[] musicfile;
    delete[] musicdonecmd;
    musicfile = musicdonecmd = nullptr;

    if(music)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(music);
        music = nullptr;
    }
    if(musicrw)
    {
        SDL_FreeRW(musicrw);
        musicrw = nullptr;
    }
    if(musicstream)
    {
        delete musicstream;
        musicstream = nullptr;
    }
}

#ifdef WIN32
    #define AUDIODRIVER "directsound winmm"
#else
    #define AUDIODRIVER "pulseaudio alsa arts esd jack pipewire dsp"
#endif

bool shouldinitaudio = true;

SVARF(audiodriver, AUDIODRIVER, { shouldinitaudio = true; initwarning("sound configuration", Init_Reset, Change_Sound); });

//master sound toggle
VARF(sound, 0, 1, 1,
{
    shouldinitaudio = true;
    initwarning("sound configuration", Init_Reset, Change_Sound);
});

//# of sound channels (not physical output channels, but individual sound samples in use, such as weaps and light ents)
VARF(soundchans, 1, 32, 128, initwarning("sound configuration", Init_Reset, Change_Sound));

//max sound frequency (44.1KHz = CD)
VARF(soundfreq, 0, 44100, 48000, initwarning("sound configuration", Init_Reset, Change_Sound));

//length of sound buffer in milliseconds
VARF(soundbufferlen, 128, 1024, 4096, initwarning("sound configuration", Init_Reset, Change_Sound));

static bool initaudio()
{
    static string fallback = "";
    static bool initfallback = true;
    if(initfallback)
    {
        initfallback = false;
        if(char *env = SDL_getenv("SDL_AUDIODRIVER"))
        {
            copystring(fallback, env);
        }
    }
    if(!fallback[0] && audiodriver[0])
    {
        std::vector<char*> drivers;
        explodelist(audiodriver, drivers);
        for(uint i = 0; i < drivers.size(); i++)
        {
            SDL_setenv("SDL_AUDIODRIVER", drivers[i], 1);
            if(SDL_InitSubSystem(SDL_INIT_AUDIO) >= 0)
            {
                for(char* j : drivers)
                {
                    delete[] j;
                }
                return true;
            }
        }
        for(char* j : drivers)
        {
            delete[] j;
        }
    }
    SDL_setenv("SDL_AUDIODRIVER", fallback, 1);
    if(SDL_InitSubSystem(SDL_INIT_AUDIO) >= 0)
    {
        return true;
    }
    conoutf(Console_Error, "sound init failed: %s", SDL_GetError());
    return false;
}

//used in iengine
void initsound()
{
    //get sdl version info
    SDL_version version;
    SDL_GetVersion(&version);
    //SDL 2.0.6 error check: 2.0.6 audio doesn't work
    if(version.major == 2 && version.minor == 0 && version.patch == 6)
    {
        nosound = true;
        if(sound)
        {
            conoutf(Console_Error, "audio is broken in SDL 2.0.6");
        }
        return;
    }

    if(shouldinitaudio)
    {
        shouldinitaudio = false; //don't init more than once
        if(SDL_WasInit(SDL_INIT_AUDIO))
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
        if(!sound || !initaudio())
        {
            nosound = true;
            return;
        }
    }

    if(Mix_OpenAudio(soundfreq, MIX_DEFAULT_FORMAT, 2, soundbufferlen)<0)
    {
        nosound = true;
        conoutf(Console_Error, "sound init failed (SDL_mixer): %s", Mix_GetError());
        return;
    }
    Mix_AllocateChannels(soundchans);
    maxchannels = soundchans;
    nosound = false;
}

//clears and deletes cached music for playback
static void musicdone()
{
    if(music)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(music);
        music = nullptr;
    }
    if(musicrw)
    {
        SDL_FreeRW(musicrw);
        musicrw = nullptr;
    }
    if(musicstream)
    {
        delete musicstream;
        musicstream = nullptr;
    }
    delete[] musicfile;
    musicfile = nullptr;
    if(!musicdonecmd)
    {
        return;
    }
    char *cmd = musicdonecmd;
    musicdonecmd = nullptr;
    execute(cmd);
    delete[] cmd;
    cmd = nullptr;
}

//uses Mix_Music object from libSDL
static Mix_Music *loadmusic(const char *name)
{
    if(!musicstream)
    {
        musicstream = openzipfile(name, "rb");
    }
    if(musicstream)
    {
        if(!musicrw)
        {
            musicrw = musicstream->rwops();
        }
        if(!musicrw)
        {
            if(musicstream)
            {
                delete musicstream;
                musicstream = nullptr;
            }
        }
    }
    if(musicrw)
    {
        music = Mix_LoadMUSType_RW(musicrw, MUS_NONE, 0);
    }
    else
    {
        music = Mix_LoadMUS(findfile(name, "rb"));
    }
    if(!music)
    {
        if(musicrw)
        {
            SDL_FreeRW(musicrw);
            musicrw = nullptr;
        }
        if(musicstream)
        {
            delete musicstream;
            musicstream = nullptr;
        }
    }
    return music;
}

//cmd
static void startmusic(char *name, char *cmd)
{
    if(nosound)
    {
        return;
    }
    stopmusic();
    if(soundvol && musicvol && *name) //if volume > 0 and music name passed
    {
        DEF_FORMAT_STRING(file, "media/%s", name);
        path(file);
        if(loadmusic(file))
        {
            delete[] musicfile;
            delete[] musicdonecmd;

            musicfile = newstring(file);
            if(cmd[0])
            {
                musicdonecmd = newstring(cmd);
            }
            else
            {
                musicdonecmd = nullptr;
            }
            Mix_PlayMusic(music, cmd[0] ? 0 : -1);
            Mix_VolumeMusic((musicvol*MIX_MAX_VOLUME)/255);
            intret(1);
        }
        else //note that there is no error message for  soundvol/musicvol/name null
        {
            conoutf(Console_Error, "could not play music: %s", file);
            intret(0);
        }
    }
}

static Mix_Chunk *loadwav(const char *name)
{
    Mix_Chunk *c = nullptr;
    stream *z = openzipfile(name, "rb");
    if(z)
    {
        SDL_RWops *rw = z->rwops();
        if(rw)
        {
            c = Mix_LoadWAV_RW(rw, 0);
            SDL_FreeRW(rw);
        }
        delete z;
    }
    if(!c)
    {
        c = Mix_LoadWAV(findfile(name, "rb"));
    }
    return c;
}

bool SoundSample::load(const char *dir)
{
    if(chunk)
    {
        return true;
    }
    if(!name[0])
    {
        return false;
    }
    static const char * const exts[] = { "", ".ogg" };
    string filename;
    for(int i = 0; i < static_cast<int>(sizeof(exts)/sizeof(exts[0])); ++i)
    {
        formatstring(filename, "media/sound/%s%s%s", dir, name, exts[i]);
        path(filename);
        chunk = loadwav(filename);
        if(chunk)
        {
            return true;
        }
    }
    conoutf(Console_Error, "failed to load sample: media/sound/%s%s", dir, name);
    return false;
}

static struct SoundType
{
    std::map<std::string, SoundSample> samples;
    std::vector<soundslot> slots;
    std::vector<SoundConfig> configs;
    const char *dir;
    SoundType(const char *dir) : dir(dir) {}
    int findsound(const char *name, int vol)
    {
        for(uint i = 0; i < configs.size(); i++)
        {
            SoundConfig &s = configs[i];
            for(int j = 0; j < s.numslots; ++j)
            {
                soundslot &c = slots[s.slots+j];
                if(!std::strcmp(c.sample->name, name) && (!vol || c.volume==vol))
                {
                    return i;
                }
            }
        }
        return -1;
    }
    int addslot(const char *name, int vol)
    {
        SoundSample * sample = nullptr;
        auto itr = samples.find(std::string(name));
        if(itr == samples.end())
        {
            char *n = newstring(name);
            SoundSample &s = samples[n];
            sample = &s;
            s.name = n;
            s.chunk = nullptr;
        }
        else
        {
            sample = &(*(itr)).second;
        }
        soundslot *oldslots = slots.data();
        int oldlen = slots.size();
        slots.emplace_back();
        soundslot &slot = slots.back();
        // soundslots.add() may relocate slot pointers
        if(slots.data() != oldslots)
        {
            for(uint i = 0; i < channels.size(); i++)
            {
                SoundChannel &chan = channels[i];
                if(chan.inuse && chan.slot >= oldslots && chan.slot < &oldslots[oldlen])
                {
                    chan.slot = &slots[chan.slot - oldslots];
                }
            }
        }
        slot.sample = sample;
        slot.volume = vol ? vol : 100;
        return oldlen;
    }
    int addsound(const char *name, int vol, int maxuses = 0)
    {
        configs.emplace_back();
        SoundConfig &s = configs.back();
        s.slots = addslot(name, vol);
        s.numslots = 1;
        s.maxuses = maxuses;
        return configs.size()-1;
    }
    void addalt(const char *name, int vol)
    {
        if(configs.empty())
        {
            return;
        }
        addslot(name, vol);
        configs.back().numslots++;
    }
    void clear()
    {
        slots.clear();
        configs.clear();
    }
    void reset() //cleanup each channel
    {
        for(uint i = 0; i < channels.size(); i++)
        {
            SoundChannel &chan = channels[i];
            soundslot * array = slots.data();
            uint size = slots.size();
            bool inbuf = chan.slot >= array + size && chan.slot < array; //within bounds of utilized vector spaces
            if(chan.inuse && inbuf)
            {
                Mix_HaltChannel(i);
                freechannel(i);
            }
        }
        clear();
    }
    void cleanupsamples()
    {
        for (auto& [k, v]: samples)
        {
            v.cleanup();
        }
    }
    void cleanup()
    {
        cleanupsamples();
        slots.clear();
        configs.clear();
        samples.clear();
    }
    void preloadsound(int n)
    {
        if(nosound || !(static_cast<long>(configs.size()) > n))
        {
            return;
        }
        SoundConfig &config = configs[n];
        for(int k = 0; k < config.numslots; ++k)
        {
            slots[config.slots+k].sample->load(dir);
        }
    }
    bool playing(const SoundChannel &chan, const SoundConfig &config) const
    {
        return chan.inuse && config.hasslot(chan.slot, slots);
    }
} gamesounds("game/"), mapsounds("mapsound/"); //init for default directories

//free all channels
static void resetchannels()
{
    for(uint i = 0; i < channels.size(); i++)
    {
        if(channels[i].inuse)
        {
            freechannel(i);
        }
    }
    channels.clear();
}

//used externally in iengine
void clear_sound()
{
    if(nosound) //don't bother closing stuff that isn't there
    {
        return;
    }
    stopmusic();

    gamesounds.cleanup();
    mapsounds.cleanup();
    Mix_CloseAudio();
    resetchannels();
}

static void stopmapsound(extentity *e)
{
    for(uint i = 0; i < channels.size(); i++)
    {
        SoundChannel &chan = channels[i];
        if(chan.inuse && chan.ent == e)
        {
            Mix_HaltChannel(i);
            freechannel(i);
        }
    }
}

VAR(stereo, 0, 1, 1); //toggles mixing of sounds by direction

//distance in cubits: how far away sound entities can be heard at(340 = 42.5m)
VAR(maxsoundradius, 1, 340, 0);

//recalculates stereo mix & volume for a soundchannel (sound ent, or player generated sound)
//(unless stereo is disabled, in which case the mix is only by distance)
bool updatechannel(SoundChannel &chan)
{
    if(!chan.slot)
    {
        return false;
    }
    int vol = soundvol,
        pan = 255/2;
    if(chan.hasloc())
    {
        vec v;
        float dist = chan.loc.dist(camera1->o, v);
        int rad = maxsoundradius;
        if(chan.ent)
        {
            rad = chan.ent->attr2;
            if(chan.ent->attr3)
            {
                rad -= chan.ent->attr3;
                dist -= chan.ent->attr3;
            }
        }
        else if(chan.radius > 0)
        {
            rad = maxsoundradius ? std::min(maxsoundradius, chan.radius) : chan.radius;
        }
        if(rad > 0) //rad = 0 means no attenuation ever
        {
            vol -= static_cast<int>(std::clamp(dist/rad, 0.0f, 1.0f)*soundvol); // simple mono distance attenuation
        }
        if(stereo && (v.x != 0 || v.y != 0) && dist>0)
        {
            v.rotate_around_z(-camera1->yaw/RAD);
            pan = static_cast<int>(255.9f*(0.5f - 0.5f*v.x/v.magnitude2())); // range is from 0 (left) to 255 (right)
        }
    }
    vol = (vol*MIX_MAX_VOLUME*chan.slot->volume)/255/255;
    vol = std::min(vol, MIX_MAX_VOLUME);
    if(vol == chan.volume && pan == chan.pan)
    {
        return false;
    }
    chan.volume = vol;
    chan.pan = pan;
    chan.dirty = true;
    return true;
}

//free channels that are not playing sounds
void reclaimchannels()
{
    for(uint i = 0; i < channels.size(); i++)
    {
        SoundChannel &chan = channels[i];
        if(chan.inuse && !Mix_Playing(i))
        {
            freechannel(i);
        }
    }
}

void syncchannels()
{
    for(uint i = 0; i < channels.size(); i++)
    {
        SoundChannel &chan = channels[i];
        if(chan.inuse && chan.hasloc() && updatechannel(chan))
        {
            syncchannel(chan);
        }
    }
}

VARP(minimizedsounds, 0, 0, 1); //toggles playing sound when window minimized

//number of sounds before the game will refuse to play another sound (with `playsound()`);
//set to 0 to disable checking (0 does not set no sounds to be playable)
VARP(maxsoundsatonce, 0, 7, 100);

VAR(debugsound, 0, 0, 1); //toggles console debug messages

void preloadsound(int n)
{
    gamesounds.preloadsound(n);
}

void preloadmapsounds()
{
    const std::vector<extentity *> &ents = entities::getents();
    for(uint i = 0; i < ents.size(); i++)
    {
        extentity &e = *ents[i];
        if(e.type==EngineEnt_Sound)
        {
            mapsounds.preloadsound(e.attr1); //load sounds by first ent attr (index)
        }
    }
}

int playsound(int n, const vec *loc = nullptr, extentity *ent = nullptr, int flags = 0, int loops = 0, int fade = 0, int chanid = -1, int radius = 0, int expire = -1)
{
    if(nosound || !soundvol || (minimized && !minimizedsounds)) //mute check
    {
        return -1;
    }
    SoundType &sounds = ent || flags&Music_Map ? mapsounds : gamesounds;
    if(!(static_cast<long>(sounds.configs.size()) > n)) //sound isn't within index
    {
        conoutf(Console_Warn, "unregistered sound: %d", n);
        return -1;
    }
    SoundConfig &config = sounds.configs[n];
    if(loc && (maxsoundradius || radius > 0))
    {
        // cull sounds that are unlikely to be heard
        //if radius is greater than zero, clamp to maxsoundradius if maxsound radius is nonzero; if radius is zero, clamp to maxsoundradius
        int rad = radius > 0 ? (maxsoundradius ? std::min(maxsoundradius, radius) : radius) : maxsoundradius;
        if(camera1->o.dist(*loc) > 1.5f*rad)
        {
            if(channels.size() > static_cast<size_t>(chanid) && sounds.playing(channels[chanid], config))
            {
                Mix_HaltChannel(chanid);
                freechannel(chanid);
            }
            return -1;
        }
    }
    if(chanid < 0)
    {
        if(config.maxuses)
        {
            int uses = 0;
            for(uint i = 0; i < channels.size(); i++)
            {
                if(sounds.playing(channels[i], config) && ++uses >= config.maxuses)
                {
                    return -1;
                }
            }
        }
        // avoid bursts of sounds with heavy packetloss and in sp
        static int soundsatonce = 0,
                   lastsoundmillis = 0;
        if(totalmillis == lastsoundmillis)
        {
            soundsatonce++;
        }
        else
        {
            soundsatonce = 1;
        }
        lastsoundmillis = totalmillis;
        if(maxsoundsatonce && soundsatonce > maxsoundsatonce)
        {
            return -1;
        }
    }
    if(channels.size() > chanid)
    {
        SoundChannel &chan = channels[chanid];
        if(sounds.playing(chan, config))
        {
            if(loc)
            {
                chan.loc = *loc;
            }
            else if(chan.hasloc())
            {
                chan.clearloc();
            }
            return chanid;
        }
    }
    if(fade < 0) //attenuation past zero
    {
        return -1;
    }
    soundslot &slot = sounds.slots[config.chooseslot(flags)];
    if(!slot.sample->chunk && !slot.sample->load(sounds.dir))
    {
        return -1;
    }
    if(debugsound)
    {
        conoutf("sound: %s%s", sounds.dir, slot.sample->name);
    }
    chanid = -1;
    for(uint i = 0; i < channels.size(); i++)
    {
        if(!channels[i].inuse)
        {
            chanid = i;
            break;
        }
    }
    if(chanid < 0 && static_cast<long>(channels.size()) < maxchannels)
    {
        chanid = channels.size();
    }
    if(chanid < 0)
    {
        for(uint i = 0; i < channels.size(); i++)
        {
            if(!channels[i].volume)
            {
                Mix_HaltChannel(i);
                freechannel(i);
                chanid = i;
                break;
            }
        }
    }
    if(chanid < 0)
    {
        return -1;
    }
    SoundChannel &chan = newchannel(chanid, &slot, loc, ent, flags, radius);
    updatechannel(chan);
    int playing = -1;
    //some ugly ternary assignments
    if(fade)
    {
        Mix_Volume(chanid, chan.volume);
        playing = expire >= 0 ?
                  Mix_FadeInChannelTimed(chanid, slot.sample->chunk, loops, fade, expire) :
                  Mix_FadeInChannel(chanid, slot.sample->chunk, loops, fade);
    }
    else
    {
        playing = expire >= 0 ?
                  Mix_PlayChannelTimed(chanid, slot.sample->chunk, loops, expire) :
                  Mix_PlayChannel(chanid, slot.sample->chunk, loops);
    }
    if(playing >= 0)
    {
        syncchannel(chan);
    }
    else
    {
        freechannel(chanid);
    }
    return playing;
}

bool stopsound(int n, int chanid, int fade)
{
    if(!(static_cast<long>(gamesounds.configs.size()) > n) || !(static_cast<long>(channels.size()) > chanid) || !gamesounds.playing(channels[chanid], gamesounds.configs[n]))
    {
        return false;
    }
    if(debugsound)
    {
        conoutf("stopsound: %s%s", gamesounds.dir, channels[chanid].slot->sample->name);
    }
    if(!fade || !Mix_FadeOutChannel(chanid, fade)) //clear and free channel allocation
    {
        Mix_HaltChannel(chanid);
        freechannel(chanid);
    }
    return true;
}

void stopmapsounds()
{
    for(uint i = 0; i < channels.size(); i++)
    {
        if(channels[i].inuse && channels[i].ent)
        {
            Mix_HaltChannel(i);
            freechannel(i);
        }
    }
}

//check map entities to see what sounds need to be played because of them
void checkmapsounds()
{
    const std::vector<extentity *> &ents = entities::getents();
    for(uint i = 0; i < ents.size(); i++)
    {
        extentity &e = *ents[i];
        if(e.type!=EngineEnt_Sound) //ents that aren't soundents don't make sound (!)
        {
            continue;
        }
        if(camera1->o.dist(e.o) < e.attr2) //if distance to entity < ent attr 2 (radius)
        {
            if(!(e.flags&EntFlag_Sound))
            {
                playsound(e.attr1, nullptr, &e, Music_Map, -1);
            }
        }
        else if(e.flags&EntFlag_Sound)
        {
            stopmapsound(&e);
        }
    }
}

void stopsounds()
{
    for(uint i = 0; i < channels.size(); i++)
    {
        if(channels[i].inuse)
        {
            Mix_HaltChannel(i);
            freechannel(i);
        }
    }
}

void updatesounds()
{
    if(nosound) //don't update sounds if disabled
    {
        return;
    }
    if(minimized && !minimizedsounds)//minimizedsounds check
    {
        stopsounds();
    }
    else
    {
        reclaimchannels(); //cull channels first
        if(mainmenu) //turn off map sounds if you reach main menu
        {
            stopmapsounds();
        }
        else
        {
            checkmapsounds();
        }
        syncchannels();
    }
    if(music)
    {
        if(!Mix_PlayingMusic())
        {
            musicdone();
        }
        else if(Mix_PausedMusic())
        {
            Mix_ResumeMusic();
        }
    }
}

int playsoundname(const char *s, const vec *loc, int vol, int flags, int loops, int fade, int chanid, int radius, int expire)
{
    if(!vol) //default to 100 volume
    {
        vol = 100;
    }
    int id = gamesounds.findsound(s, vol);
    if(id < 0)
    {
        id = gamesounds.addsound(s, vol);
    }
    return playsound(id, loc, nullptr, flags, loops, fade, chanid, radius, expire);
}

void resetsound()
{
    clearchanges(Change_Sound);
    if(!nosound)
    {
        gamesounds.cleanupsamples();
        mapsounds.cleanupsamples();
        if(music)
        {
            Mix_HaltMusic();
            Mix_FreeMusic(music);
        }
        if(musicstream)
        {
            musicstream->seek(0, SEEK_SET);
        }
        Mix_CloseAudio();
    }
    initsound();
    resetchannels();
    if(nosound) //clear stuff if muted
    {
        delete[] musicfile;
        delete[] musicdonecmd;

        musicfile = musicdonecmd = nullptr;
        music = nullptr;
        gamesounds.cleanupsamples();
        mapsounds.cleanupsamples();
        return;
    }
    if(music && loadmusic(musicfile))
    {
        Mix_PlayMusic(music, musicdonecmd ? 0 : -1);
        Mix_VolumeMusic((musicvol*MIX_MAX_VOLUME)/255);
    }
    else
    {
        delete[] musicfile;
        delete[] musicdonecmd;

        musicfile = musicdonecmd = nullptr;
    }
}

void initsoundcmds()
{

    addcommand("music", reinterpret_cast<identfun>(startmusic), "ss", Id_Command);
    addcommand("playsound", reinterpret_cast<identfun>(playsound), "i", Id_Command); //i: the index of the sound to be played
    addcommand("resetsound", reinterpret_cast<identfun>(resetsound), "", Id_Command); //stop all sounds and re-play background music

    static auto registersound = [] (char *name, int *vol)
    {
        intret(gamesounds.addsound(name, *vol, 0));
    };

    static auto mapsound = [] (char *name, int *vol, int *maxuses)
    {
        intret(mapsounds.addsound(name, *vol, *maxuses < 0 ? 0 : std::max(1, *maxuses)));
    };

    static auto altsound = [] (char *name, int *vol)
    {
        gamesounds.addalt(name, *vol);
    };

    static auto altmapsound = [] (char *name, int *vol)
    {
        mapsounds.addalt(name, *vol);
    };

    static auto numsounds = [] ()
    {
        intret(gamesounds.configs.size());
    };

    static auto nummapsounds = [] ()
    {
        intret(mapsounds.configs.size());
    };

    static auto soundreset = [] ()
    {
        gamesounds.reset();
    };

    static auto mapsoundreset = [] ()
    {
        mapsounds.reset();
    };

    addcommand("registersound", reinterpret_cast<identfun>(+registersound), "si", Id_Command);
    addcommand("mapsound", reinterpret_cast<identfun>(+mapsound), "sii", Id_Command);
    addcommand("altsound", reinterpret_cast<identfun>(+altsound), "si", Id_Command);
    addcommand("altmapsound", reinterpret_cast<identfun>(+altmapsound), "si", Id_Command);
    addcommand("numsounds", reinterpret_cast<identfun>(+numsounds), "", Id_Command);
    addcommand("nummapsounds", reinterpret_cast<identfun>(+nummapsounds), "", Id_Command);
    addcommand("soundreset", reinterpret_cast<identfun>(+soundreset), "", Id_Command);
    addcommand("mapsoundreset", reinterpret_cast<identfun>(+mapsoundreset), "", Id_Command);

}
