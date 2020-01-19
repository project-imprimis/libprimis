# Imprimis

### An Open Source Team Shooter

## Gameplay and Interface Design Document

The contents of this design document may be subject to change at any time.

This document does not cover any technical decisions, such as the UI system, engine, or networking.

## 0. Philosophy

Imprimis aims to be a focused, exhaustive project to optimize gameplay on only team-based objective oriented gameplay.
The game does not and has no intention of officially supporting free-for-all gameplay nor changes to the base mode.

Imprimis is not, however, a competetive-focused game and aims to be highly accessible and as egalatarian as possible to
appeal to a broad base of players. It is not based on any particular first person shooter, but draws some of
its game flow concepts from games ranging from Quake to Team Fortress 2 to Counter-Stike.

Discussions about specific elements of gameplay are welcome, but be aware that the game, in order to remain focused,
has no intention of supporting anything that the project is unable to properly spend time to develop, playtest, and
publish in a way that provides an improvement in the user experience for the average inexperienced player.

This is perhaps considered to be a very conservative way of designing a game, but particularly for open source projects,
ambition is usually met with the realities of limited man-hours, lack of professional skill, and fickle contributors.
It is the belief of the project that mere competence and focus on providing centrist, engrossing, and accessible gameplay
is enough to allow it to find a niche in the field of open source software.

In a sense, Imprimis has no intention of being a revolutionary change in the way that first person shooters are characterized,
but that level of achievement is also not required in order to be the best open-source developed and produced shooter
to ever be made. Imprimis intends to be the most competent first person shooter it can be, of course, but the target that
the project aims for is more to be the most professional, easy to understand, and polished piece of software (both in its
user experience and source code) rather than to be the next revolution in gaming.

## 1. Gameplay Mechanics

Imprimis is a team shooter, and because it is designed from the ground up as a team shooter, has a handful of design
choices which it has choosen to undergo in order to meet specific choices in the game design.

The overall objective is to allow all players to feel they have agency in the game's outcome, and to allow coordination
and sensible use of the level's features to defeat players and teams which opt to rely solely on their physical abilities.
Physical abilities include accuracy and reaction time, and as with all FPS shooters these skills are still quite valuable in
performance ingame.

### Mechanics Are Tools

It is important to make a quick note that the game is not designed around *mechanics* but instead around *outcomes*.
Just like how artists use light and shadow to highlight their scenes or writers use tropes to deliver particular thoughts
to their audiences, Imprimis uses gameplay mechanics to deliver a set of game flow outcomes to players. This means that
suggestions such as vehicles or mechs or other (objectively cool) mechanics require a problem that they solve in order
to be considered as a part of the gameplay. Games which simply use mechanics because they are cool tend to have problems
with creating more than superficially interesting gameplay because mechanics always have balance issues which are often
difficult to tackle, particularly in aggregate.

### Moderate Mobility

Mobility is a key part of an FPS experience, as its setup affects the gameplay meta perhaps as much as any other part of
the game. High mobility allows for deep, complex gameplay interactions when done correctly, but also suffers from many
issues, including the difficulty of creating a suitable physics engine to accomodate multidimensional movement and the
difficulty of making a suitable art style to accomodate the limitations of the physics engine combined with the freedom
of movement. The biggest problem, however, is the difficulty of creating a user interface which adequately explains
movement such that the gameplay is still accessible to people who are not dedicated to learning every nuance of the
gameplay physics.

### Limited Verticality

Verticality is a very engaging feature when executed well, but the particular constraints of the Tesseract engine penalize
players which have large vertical abilities. Tesseract's most performant way of lighting comes from sunlight and diffuse
global illumination, and tall corridor walls make this particularly difficult to admit onto levels. Lighting in Tesseract
is fairly expensive because of its realtime nature, and so where necessary it is best to take the engine's particular
shortcomings into play. In addition, the shortcomings noted above about too much mobility also apply to liberal vertical
movement mechanics.

### Low TTK

Imprimis has opted to become a low time-to-kill (TTK) shooter. High TTK shooters, such as arena shooters, have problems
wherein they tend to reward selfish, uncoordinated behavior as experienced players have the ability to accomplish 
objectives and kill enemy players in relative impunity with respect to less skilled players. This is just fine for
free-for-all gameplay, where the entire purpose of the game is to maximize one's damage output per unit time, but
in the case of a team shooter, it promotes selfish and uncooporative behavior which marginalizes less skilled players to
near irrelevance. 

### Class-based Weaponry

In an effort to service the primary goal of accessibilty and simplicity whenever possible, Imprimis is designed around
a class based system. By restricting each player to one primary weapon, the game can more easily setup metas which 
prioritize finding favorable matchups while in game. This is essential to avoiding engagements from becoming even matched,
where neither side has a tactical advantage because there is no appreciable difference in the specific tactical abilities
of any arbitrary weapon loadout. An added benefit of the moderate mobility the game is designed around is the ability
to introduce per-weapon weight and speed penalties without affecting the player's ability to traverse the map effectively.

### Objective-based Gameplay 

In an attempt to promote gameplay that is inclusive and active, Imprimis is an objective-based shooter. In contrast to
standard deathmatch modes, objective-based gameplay requires more active and aggressive play which simultaneously is less
harsh on less skilled players than free-for-all or even team deathmatch game balance. In capturing an objective, players
who are perhaps less capable of keeping their kill/death ratios above unity (which is required to not be counterproductive
for your team's overall standing) have the ability to still contribute to their team's outcome in a positive way.

### Fixed Team Sizes

Because the game is simple enough for bots to do a plausible job of representing a human player, it is practical and useful
to restrict the team sizes to a single value. This has major benefits in allowing for more focused development on maps and
mechanics centered around a single size match, and allows for the game to be as balanced as possible given the amount of
development time available. This homogenizing of the gameplay in tandem with the other game design standards allows for a
minimal amount of potential game types to design immutable parts of the game around (such as movement speed and weapon
mechanics).

## 2. User Interfaces and User Experience

A particular struggle in the open-source community with respect to gaining wide adoption is mainly in the user interface
and user experience aspects of programs' design. Many of the most powerful software tools that humanity has ever made
are open source, such as the immense range of tools in Blender, a program which by itself has the tools to build entire movies.

But the number of tools have never been the problem in open source software: for this reason people who need their computers
to do advanced tasks have long run their scientific computing or continuous integration or web hosting on machines which call
their kernel Linux, running open-source libraries like Apache or SciPy or GNU Make to accomplish these tasks. But the reality
is that these tools are those of the professional, and for those without more than passing interest in the mechanisms of 
software these abilities do not sell themselves unless the user can do their tasks without knowing anything about the machinery.

For most open source software programs, this is not a particular concern, as those who find a library useful only care that
it exists, and do not mind if they are the only person in the world who has a use for it. But this project is a multiplayer
shooter, and thus suffers the trap of needing *other people* to also be present for the program to be of much interest to
users. For this reason, not compromising on the ease of use and cleanliness of the program is essential to getting the game
to appeal to a broad base, and user interface design is very important.

### Knowing Better Than The User

This is not condescending so much as it is not leaving the user out to dry: the user interface can't include spurious or
irrelevant detail. For example, the vast majority of players will not (and should not) learn enough about the game's engine
to understand the differences between antialiasing implementations. Providing these settings then, particularly without being
very clear about their performance ramifications, is misleading and results in the player using their agency to select settings
they don't actually want (because they aren't experts on the engine's mechanics like people who design the level are). Most
laypeople just want their game to work, and making sure that the game takes care of this from the start is preferable to
cluttering interfaces with meaningless techobabble.

### No Crap

The simplest way to not allow users to select crap is to not allow any crap in the first place to be in the game. This includes
game assets, maps, and gameplay mechanics and modes. Removal is preferable to marginal inclusion; a user should be able to
select any random thing from the user interface and never experience a drop in the gameplay quality. An important part of being
an organization is being organized; the project is not an amoeba which ingests whatever it may run into along its random walk.

In terms of ingame play, this also means that there will be no weapons rendered irrelevant by the gameplay meta, and extensive
testing will be done to ensure that players will be competent without learning the specifics of the gameplay mechanics.

This is why the initial release of Imprimis is set to have only one mode: it does not appear reasonable to provide a consistent
level of quality with two modes within the current game setup given the expected number of man-hours before launch. Building
the game this way means that future updates can provide meaningful expansions to the gameplay rather than fixing problems in
a never ending cascade of marginal gameplay patches, which provide nothing to get excited about for end users.

### Consistency or Nothing

Levels, modes and assets are to be consistent in their messaging such that it is possible to see that the game
tells a cohesive story about its setting. This means that the game will never have ninja stars or flamberges or other objects
irrelevant to the rest of the art style. This is important in portraying a professional, unified product. This also means
that user interface improvements or feature expansions will not be released until they are no lower quality than any of the
existing content.
