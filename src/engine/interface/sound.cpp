// sound.cpp: basic positional sound using sdl_mixer

#include "../libprimis-headers/cube.h"
#include "../libprimis-headers/sound.h"
#include "SDL_mixer.h"

#include "console.h"
#include "control.h"
#include "cs.h"
#include "input.h"
#include "menus.h"

#include "render/rendergl.h" //needed to get camera position

#include "world/entities.h"
#include "world/world.h"

SoundEngine::SoundEngine() : gamesounds("game/", *this), mapsounds("mapsound/", *this)
{
}

SoundEngine::SoundSample::SoundSample(SoundEngine& p) : parent(&p), name(""), chunk(nullptr)
{
}

SoundEngine::SoundSample::~SoundSample()
{
}

void SoundEngine::SoundSample::cleanup()
{
    if(chunk)
    {
        Mix_FreeChunk(chunk);
        chunk = nullptr;
    }
}

bool SoundEngine::SoundConfig::hasslot(const soundslot *p, const std::vector<soundslot> &v) const
{
    return p >= v.data() + slots && p < v.data() + slots+numslots && slots+numslots < static_cast<long>(v.size());
}

int SoundEngine::SoundConfig::chooseslot(int flags) const
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

SoundEngine::SoundChannel::SoundChannel(int id, SoundEngine& p) : parent(&p), id(id)
{
    reset();
}

bool SoundEngine::SoundChannel::hasloc() const
{
    return loc.x >= -1e15f;
}

void SoundEngine::SoundChannel::clearloc()
{
    loc = vec(-1e16f, -1e16f, -1e16f);
}

void SoundEngine::SoundChannel::reset()
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

void SoundEngine::SoundChannel::setloc(const vec& newloc)
{
    loc = newloc;
}

void SoundEngine::SoundChannel::setupchannel(int newn, soundslot *newslot, const vec *newloc, extentity *newent, int newflags, int newradius)
{
    reset();
    inuse = true;
    if(newloc)
    {
        loc = *newloc;
    }
    slot = newslot;
    ent = newent;
    flags = 0;
    radius = newradius;
}

//creates a new SoundChannel object with passed properties
SoundEngine::SoundChannel& SoundEngine::newchannel(int n, soundslot *slot, const vec *loc, extentity *ent, int flags, int radius)
{
    if(ent)
    {
        loc = &ent->o;
        ent->flags |= EntFlag_Sound;
    }
    while(!(static_cast<long>(channels.size()) > n))
    {
        channels.emplace_back(channels.size(), *this);
    }
    SoundChannel &chan = channels[n];
    chan.setupchannel(n, slot, loc, ent, flags, radius);
    return chan;
}

//sets a channel as not being in use
void SoundEngine::freechannel(int n)
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

void SoundEngine::SoundChannel::syncchannel()
{
    if(!dirty)
    {
        return;
    }
    if(!Mix_FadingChannel(id))
    {
        Mix_Volume(id, volume);
    }
    Mix_SetPanning(id, 255-pan, pan);
    dirty = false;
}

void SoundEngine::stopchannels()
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

void SoundEngine::setsoundvol(const int * const vol)
{
    soundvol = std::clamp(*vol, 0, 255);
    if(!vol)
    {
        stopchannels();
        setmusicvol(0);
    }
}

int SoundEngine::getsoundvol()
{
    return soundvol;
}

void SoundEngine::setmusicvol(const int * const vol)
{
    soundvol = std::clamp(*vol, 0, 255);
    setmusicvol(soundvol ? musicvol : 0);
}

int SoundEngine::getmusicvol()
{
    return musicvol;
}

void SoundEngine::setmusicvol(int musicvol)
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

void SoundEngine::stopmusic()
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

//SVARF(audiodriver, AUDIODRIVER, { shouldinitaudio = true; initwarning("sound configuration", Init_Reset, Change_Sound); });
void SoundEngine::setaudiodriver(char * f)
{
    audiodriver = std::string(f);
    shouldinitaudio = true;
    initwarning("sound configuration", Init_Reset, Change_Sound);
}

void SoundEngine::setsound(const int * const on)
{
    sound = *on;
    shouldinitaudio = true;
    initwarning("sound configuration", Init_Reset, Change_Sound);
}
int SoundEngine::getsound()
{
    if(sound)
    {
        return 1;
    }
    return 0;
}

//VARF(soundchans, 1, 32, 128, initwarning("sound configuration", Init_Reset, Change_Sound));
void SoundEngine::setsoundchans(const int * const val)
{
    soundchans = std::clamp(*val, 1, 128);
    initwarning("sound configuration", Init_Reset, Change_Sound);
}
int SoundEngine::getsoundchans()
{
    return soundchans;
}

bool SoundEngine::initaudio()
{
    static std::string fallback = "";
    static bool initfallback = true;
    if(initfallback)
    {
        initfallback = false;
        if(const char *env = SDL_getenv("SDL_AUDIODRIVER"))
        {
            fallback = std::string(env);
        }
    }
    if(!fallback[0] && audiodriver[0])
    {
        std::vector<char*> drivers;
        explodelist(audiodriver.c_str(), drivers);
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
    SDL_setenv("SDL_AUDIODRIVER", fallback.c_str(), 1);
    if(SDL_InitSubSystem(SDL_INIT_AUDIO) >= 0)
    {
        return true;
    }
    conoutf(Console_Error, "sound init failed: %s", SDL_GetError());
    return false;
}

//used in iengine
void SoundEngine::initsound()
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
void SoundEngine::musicdone()
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
Mix_Music *SoundEngine::loadmusic(const char *name)
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
void SoundEngine::startmusic(char *name, char *cmd)
{
    if(nosound)
    {
        return;
    }
    stopmusic();
    if(soundvol && musicvol && *name) //if volume > 0 and music name passed
    {
        std::string file = "media/";
        file.append(name);
        path(file);
        if(loadmusic(file.c_str()))
        {
            delete[] musicfile;
            delete[] musicdonecmd;

            musicfile = newstring(file.c_str());
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
            conoutf(Console_Error, "could not play music: %s", file.c_str());
            intret(0);
        }
    }
}

Mix_Chunk *SoundEngine::loadwav(const char *name)
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

bool SoundEngine::SoundSample::load(const char *dir)
{
    if(chunk)
    {
        return true;
    }
    if(!name.size())
    {
        return false;
    }
    static const char * const exts[] = { "", ".ogg" };
    string filename;
    for(int i = 0; i < static_cast<int>(sizeof(exts)/sizeof(exts[0])); ++i)
    {
        formatstring(filename, "media/sound/%s%s%s", dir, name.c_str(), exts[i]);
        path(filename);
        chunk = parent->loadwav(filename);
        if(chunk)
        {
            return true;
        }
    }
    conoutf(Console_Error, "failed to load sample: media/sound/%s%s", dir, name.c_str());
    return false;
}

//SoundType
SoundEngine::SoundType::SoundType(const char *dir, SoundEngine& p) : parent(&p), dir(dir) {}

int SoundEngine::SoundType::findsound(const char *name, int vol)
{
    for(uint i = 0; i < configs.size(); i++)
    {
        SoundConfig &s = configs[i];
        for(int j = 0; j < s.numslots; ++j)
        {
            soundslot &c = slots[s.slots+j];
            if(!std::strcmp(c.sample->name.c_str(), name) && (!vol || c.volume==vol))
            {
                return i;
            }
        }
    }
    return -1;
}
int SoundEngine::SoundType::addslot(const char *name, int vol)
{
    SoundSample * sample = nullptr;
    auto itr = samples.find(std::string(name));
    if(itr == samples.end())
    {
        SoundSample s(*parent);
        s.name = std::string(name);
        s.chunk = nullptr;
        samples.insert(std::pair<std::string, SoundSample>(std::string(name), s));
        itr = samples.find(std::string(name));
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
        for(uint i = 0; i < parent->channels.size(); i++)
        {
            SoundChannel &chan = parent->channels[i];
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
int SoundEngine::SoundType::addsound(const char *name, int vol, int maxuses)
{
    SoundConfig s;
    s.slots = addslot(name, vol);
    s.numslots = 1;
    s.maxuses = maxuses;
    configs.push_back(s);
    return configs.size()-1;
}

void SoundEngine::SoundType::addalt(const char *name, int vol)
{
    if(configs.empty())
    {
        return;
    }
    addslot(name, vol);
    configs.back().numslots++;
}
void SoundEngine::SoundType::clear()
{
    slots.clear();
    configs.clear();
}
void SoundEngine::SoundType::reset() //cleanup each channel
{
    for(uint i = 0; i < parent->channels.size(); i++)
    {
        SoundChannel &chan = parent->channels[i];
        soundslot * array = slots.data();
        uint size = slots.size();
        bool inbuf = chan.slot >= array + size && chan.slot < array; //within bounds of utilized vector spaces
        if(chan.inuse && inbuf)
        {
            Mix_HaltChannel(i);
            parent->freechannel(i);
        }
    }
    clear();
}
void SoundEngine::SoundType::cleanupsamples()
{
    for (auto& [k, v]: samples)
    {
        v.cleanup();
    }
}
void SoundEngine::SoundType::cleanup()
{
    cleanupsamples();
    slots.clear();
    configs.clear();
    samples.clear();
}
void SoundEngine::SoundType::preloadsound(int n)
{
    if(parent->nosound || !(static_cast<long>(configs.size()) > n))
    {
        return;
    }
    SoundConfig &config = configs[n];
    for(int k = 0; k < config.numslots; ++k)
    {
        slots[config.slots+k].sample->load(dir);
    }
}
bool SoundEngine::SoundType::playing(const SoundChannel &chan, const SoundConfig &config) const
{
    return chan.inuse && config.hasslot(chan.slot, slots);
}

//free all channels
void SoundEngine::resetchannels()
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
void SoundEngine::clear_sound()
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

void SoundEngine::stopmapsound(extentity *e)
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

void SoundEngine::setstereo(const int * const on)
{
    stereo = on;
}
int SoundEngine::getstereo()
{
    if(stereo)
    {
        return 1;
    }
    return 0;
}
//VAR(stereo, 0, 1, 1); //toggles mixing of sounds by direction

//distance in cubits: how far away sound entities can be heard at(340 = 42.5m)
void SoundEngine::setmaxradius(const int * const dist)
{
    maxsoundradius = *dist;
}
int SoundEngine::getmaxradius()
{
    return maxsoundradius;
}

//recalculates stereo mix & volume for a soundchannel (sound ent, or player generated sound)
//(unless stereo is disabled, in which case the mix is only by distance)
bool SoundEngine::SoundChannel::updatechannel()
{
    if(!slot)
    {
        return false;
    }
    int vol = parent->soundvol,
        middlepan = 255/2;
    if(hasloc())
    {
        vec v;
        float dist = loc.dist(camera1->o, v);
        int rad = parent->maxsoundradius;
        if(ent)
        {
            rad = ent->attr2;
            if(ent->attr3)
            {
                rad -= ent->attr3;
                dist -= ent->attr3;
            }
        }
        else if(radius > 0)
        {
            rad = parent->maxsoundradius ? std::min(parent->maxsoundradius, radius) : radius;
        }
        if(rad > 0) //rad = 0 means no attenuation ever
        {
            vol -= static_cast<int>(std::clamp(dist/rad, 0.0f, 1.0f)*parent->soundvol); // simple mono distance attenuation
        }
        if(parent->stereo && (v.x != 0 || v.y != 0) && dist>0)
        {
            v.rotate_around_z(-camera1->yaw/RAD);
            pan = static_cast<int>(255.9f*(0.5f - 0.5f*v.x/v.magnitude2())); // range is from 0 (left) to 255 (right)
        }
    }
    vol = (vol*MIX_MAX_VOLUME*slot->volume)/255/255;
    vol = std::min(vol, MIX_MAX_VOLUME);
    if(vol == volume && pan == middlepan)
    {
        return false;
    }
    volume = vol;
    pan = middlepan;
    dirty = true;
    return true;
}

//free channels that are not playing sounds
void SoundEngine::reclaimchannels()
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

void SoundEngine::syncchannels()
{
    for(uint i = 0; i < channels.size(); i++)
    {
        SoundChannel &chan = channels[i];
        if(chan.inuse && chan.hasloc() && chan.updatechannel())
        {
            chan.syncchannel();
        }
    }
}

//VARP(minimizedsounds, 0, 0, 1); //toggles playing sound when window minimized
int SoundEngine::getminimizedsounds()
{
    return minimizedsounds;
}
void SoundEngine::setminimizedsounds(int minimize)
{
    minimizedsounds = minimize;
}

//number of sounds before the game will refuse to play another sound (with `playsound()`);
//set to 0 to disable checking (0 does not set no sounds to be playable)
//VARP(maxsoundsatonce, 0, 7, 100);
int SoundEngine::getmaxsoundsatonce()
{
    return maxsoundsatonce;
}
void SoundEngine::setmaxsoundsatonce(const int * num)
{
    maxsoundsatonce = std::clamp(*num, 0, 100);
}

//used in iengine.h
void SoundEngine::preloadsound(int n)
{
    gamesounds.preloadsound(n);
}

void SoundEngine::preloadmapsounds()
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

//used in iengine.h
int SoundEngine::playsound(int n, const vec *loc, extentity *ent, int flags, int loops, int fade, int chanid, int radius, int expire)
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
            for(const SoundChannel &s : channels)
            {
                if(sounds.playing(s, config) && ++uses >= config.maxuses)
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
    if(channels.size() > static_cast<size_t>(chanid))
    {
        SoundChannel &chan = channels[chanid];
        if(sounds.playing(chan, config))
        {
            if(loc)
            {
                chan.setloc(*loc);
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
        conoutf("sound: %s%s", sounds.dir, slot.sample->name.c_str());
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
    chan.updatechannel();
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
        chan.syncchannel();
    }
    else
    {
        freechannel(chanid);
    }
    return playing;
}

bool SoundEngine::stopsound(int n, int chanid, int fade)
{
    if(!(static_cast<long>(gamesounds.configs.size()) > n) || !(static_cast<long>(channels.size()) > chanid) || !gamesounds.playing(channels[chanid], gamesounds.configs[n]))
    {
        return false;
    }
    if(debugsound)
    {
        conoutf("stopsound: %s%s", gamesounds.dir, channels[chanid].slot->sample->name.c_str());
    }
    if(!fade || !Mix_FadeOutChannel(chanid, fade)) //clear and free channel allocation
    {
        Mix_HaltChannel(chanid);
        freechannel(chanid);
    }
    return true;
}

void SoundEngine::stopmapsounds()
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
void SoundEngine::checkmapsounds()
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

void SoundEngine::stopsounds()
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

void SoundEngine::updatesounds()
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

//used in iengine.h
int SoundEngine::playsoundname(const char *s, const vec *loc, int vol, int flags, int loops, int fade, int chanid, int radius, int expire)
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

void SoundEngine::resetsound()
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

void SoundEngine::registersound (char *name, int *vol)
{
    intret(gamesounds.addsound(name, *vol, 0));
}

void SoundEngine::mapsound(char *name, int *vol, int *maxuses)
{
    intret(mapsounds.addsound(name, *vol, *maxuses < 0 ? 0 : std::max(1, *maxuses)));
}

void SoundEngine::altsound(char *name, int *vol)
{
    gamesounds.addalt(name, *vol);
}

void SoundEngine::altmapsound(char *name, int *vol)
{
    mapsounds.addalt(name, *vol);
}

void SoundEngine::numsounds()
{
    intret(gamesounds.configs.size());
}

void SoundEngine::nummapsounds()
{
    intret(mapsounds.configs.size());
}

void SoundEngine::soundreset()
{
    gamesounds.reset();
}

void SoundEngine::mapsoundreset()
{
    mapsounds.reset();
}
