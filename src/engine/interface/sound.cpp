// sound.cpp: basic positional sound using sdl_mixer

#include "../libprimis-headers/cube.h"

#include "SDL_mixer.h"

#include "console.h"
#include "control.h"
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

    bool hasslot(const soundslot *p, const vector<soundslot> &v) const
    {
        return p >= v.getbuf() + slots && p < v.getbuf() + slots+numslots && slots+numslots < v.length();
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
vector<SoundChannel> channels;
int maxchannels = 0;

//creates a new SoundChannel object with passed properties
SoundChannel &newchannel(int n, soundslot *slot, const vec *loc = nullptr, extentity *ent = nullptr, int flags = 0, int radius = 0)
{
    if(ent)
    {
        loc = &ent->o;
        ent->flags |= EntFlag_Sound;
    }
    while(!channels.inrange(n))
    {
        channels.add(channels.length());
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
void freechannel(int n)
{
    if(!channels.inrange(n) || !channels[n].inuse)
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

void syncchannel(SoundChannel &chan)
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

void stopchannels()
{
    for(int i = 0; i < channels.length(); i++)
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

void setmusicvol(int musicvol);

VARFP(soundvol, 0, 255, 255,
    if(!soundvol)
    { //don't use sound infrastructure if volume is 0
        stopchannels();
        setmusicvol(0);
    }
);

VARFP(musicvol, 0, 60, 255, setmusicvol(soundvol ? musicvol : 0)); //background music volume

char *musicfile    = nullptr,
     *musicdonecmd = nullptr;

Mix_Music *music    = nullptr;
SDL_RWops *musicrw  = nullptr;
stream *musicstream = nullptr;

void setmusicvol(int musicvol)
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

void stopmusic()
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

bool initaudio()
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
        vector<char*> drivers;
        explodelist(audiodriver, drivers);
        for(int i = 0; i < drivers.length(); i++)
        {
            SDL_setenv("SDL_AUDIODRIVER", drivers[i], 1);
            if(SDL_InitSubSystem(SDL_INIT_AUDIO) >= 0)
            {
                drivers.deletearrays();
                return true;
            }
        }
        drivers.deletearrays();
    }
    SDL_setenv("SDL_AUDIODRIVER", fallback, 1);
    if(SDL_InitSubSystem(SDL_INIT_AUDIO) >= 0)
    {
        return true;
    }
    conoutf(Console_Error, "sound init failed: %s", SDL_GetError());
    return false;
}

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
void musicdone()
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
Mix_Music *loadmusic(const char *name)
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

void startmusic(char *name, char *cmd)
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
COMMANDN(music, startmusic, "ss");

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
    hashnameset<SoundSample> samples;
    vector<soundslot> slots;
    vector<SoundConfig> configs;
    const char *dir;
    SoundType(const char *dir) : dir(dir) {}
    int findsound(const char *name, int vol)
    {
        for(int i = 0; i < configs.length(); i++)
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
        SoundSample *s = samples.access(name);
        if(!s)
        {
            char *n = newstring(name);
            s = &samples[n];
            s->name = n;
            s->chunk = nullptr;
        }
        soundslot *oldslots = slots.getbuf();
        int oldlen = slots.length();
        soundslot &slot = slots.add();
        // soundslots.add() may relocate slot pointers
        if(slots.getbuf() != oldslots)
        {
            for(int i = 0; i < channels.length(); i++)
            {
                SoundChannel &chan = channels[i];
                if(chan.inuse && chan.slot >= oldslots && chan.slot < &oldslots[oldlen])
                {
                    chan.slot = &slots[chan.slot - oldslots];
                }
            }
        }
        slot.sample = s;
        slot.volume = vol ? vol : 100;
        return oldlen;
    }
    int addsound(const char *name, int vol, int maxuses = 0)
    {
        SoundConfig &s = configs.add();
        s.slots = addslot(name, vol);
        s.numslots = 1;
        s.maxuses = maxuses;
        return configs.length()-1;
    }
    void addalt(const char *name, int vol)
    {
        if(configs.empty())
        {
            return;
        }
        addslot(name, vol);
        configs.last().numslots++;
    }
    void clear()
    {
        slots.setsize(0);
        configs.setsize(0);
    }
    void reset() //cleanup each channel
    {
        for(int i = 0; i < channels.length(); i++)
        {
            SoundChannel &chan = channels[i];
            if(chan.inuse && slots.inbuf(chan.slot))
            {
                Mix_HaltChannel(i);
                freechannel(i);
            }
        }
        clear();
    }
    void cleanupsamples()
    {
        ENUMERATE(samples, SoundSample, s, s.cleanup());
    }
    void cleanup()
    {
        cleanupsamples();
        slots.setsize(0);
        configs.setsize(0);
        samples.clear();
    }
    void preloadsound(int n)
    {
        if(nosound || !configs.inrange(n))
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

void registersound(char *name, int *vol)
{
    intret(gamesounds.addsound(name, *vol, 0));
}
COMMAND(registersound, "si");

void mapsound(char *name, int *vol, int *maxuses)
{
    intret(mapsounds.addsound(name, *vol, *maxuses < 0 ? 0 : std::max(1, *maxuses)));
}
COMMAND(mapsound, "sii");

void altsound(char *name, int *vol)
{
    gamesounds.addalt(name, *vol);
}
COMMAND(altsound, "si");

void altmapsound(char *name, int *vol)
{
    mapsounds.addalt(name, *vol);
}
COMMAND(altmapsound, "si");

void numsounds()
{
    intret(gamesounds.configs.length());
}
COMMAND(numsounds, "");

void nummapsounds()
{
    intret(mapsounds.configs.length());
}
COMMAND(nummapsounds, "");

void soundreset()
{
    gamesounds.reset();
}
COMMAND(soundreset, "");

void mapsoundreset()
{
    mapsounds.reset();
}
COMMAND(mapsoundreset, "");

//free all channels
void resetchannels()
{
    for(int i = 0; i < channels.length(); i++)
    {
        if(channels[i].inuse)
        {
            freechannel(i);
        }
    }
    channels.shrink(0);
}

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

void stopmapsound(extentity *e)
{
    for(int i = 0; i < channels.length(); i++)
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
            v.rotate_around_z(-camera1->yaw*RAD);
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
    for(int i = 0; i < channels.length(); i++)
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
    for(int i = 0; i < channels.length(); i++)
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

void preloadmapsound(int n)
{
    mapsounds.preloadsound(n);
}

void preloadmapsounds()
{
    const vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < ents.length(); i++)
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
    if(!sounds.configs.inrange(n)) //sound isn't within index
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
            if(channels.inrange(chanid) && sounds.playing(channels[chanid], config))
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
            for(int i = 0; i < channels.length(); i++)
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
    if(channels.inrange(chanid))
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
    for(int i = 0; i < channels.length(); i++)
    {
        if(!channels[i].inuse)
        {
            chanid = i;
            break;
        }
    }
    if(chanid < 0 && channels.length() < maxchannels)
    {
        chanid = channels.length();
    }
    if(chanid < 0)
    {
        for(int i = 0; i < channels.length(); i++)
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
COMMAND(playsound, "i"); //i: the index of the sound to be played

bool stopsound(int n, int chanid, int fade)
{
    if(!gamesounds.configs.inrange(n) || !channels.inrange(chanid) || !gamesounds.playing(channels[chanid], gamesounds.configs[n]))
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
    for(int i = 0; i < channels.length(); i++)
    {
        if(channels[i].inuse && channels[i].ent)
        {
            Mix_HaltChannel(i);
            freechannel(i);
        }
    }
}

void clearmapsounds()
{
    stopmapsounds();
    mapsounds.clear();
}

//check map entities to see what sounds need to be played because of them
void checkmapsounds()
{
    const vector<extentity *> &ents = entities::getents();
    for(int i = 0; i < ents.length(); i++)
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
    for(int i = 0; i < channels.length(); i++)
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
COMMAND(resetsound, ""); //stop all sounds and re-play background music
