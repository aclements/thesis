PDOS practice talk 2013-10-30
-----------------------------

### Xi high-level ###

First two slides too long, too bashing

* This talk is about answering questions

 * "Is there a scalable implementation of ..."

High-level roadmap

* Sort of like contributions slide, but better connected.  What you
  can do with this.

Challenges come late


### Slide by slide ###

Talk faster at beginning


Downsides

* Too long, more concise

* Maybe remove sub-bullets?


Goal

* Tie together better with downsides

 * Might help simplify


Contributions

* Present as a roadmap for how to use the rule, ans also a talk
  roadmap


Modern multicore hardware

* Shrink figure?

* Need a transition from previous slide

 * What is a good definition for scalability?

* Present hardware before any definition of scalability, then present
  both definitions of scalability next to each other

 * We could measure it, but there's a reason.  There are other
   reasons, but we're going to focus on cache-line sharing and that's
   going to be our definition

* Let's look at how scalability problems manifest on hardware

* Maybe put bang on graph slide?

* Reads can happen in parallel


Mention early that I'm going to use POSIX as an interface.  Also makes
it clear what we mean by interface


What interface have scalable implementations?

* Diagram of concurrent hash table instead of just talking through it

* Diagram of shared FD table

* Xi wants this slide to be first

 * Make it concrete, shows people what we mean by "interface"

* Maybe write down reasons or show pictures instead of just speaking


SIM commutativity

* Pull in creat example from previous slide

* Maybe say it generalizes to more than two operations


The Scalable Commutativity Rule #2

* Give intuition

* Define "conflict-free" or just use expansion?

* Give why for "Yes" and "No"


Observations

* Remove first bullet and example


Applying to rule

* Typo in title

* Relate first two lines better so they don't look like four
  independent sentences

 * Arrows?


Example: rename

* Fix indentation (when rewinding slide, loses transformation)

* Point out that this is really a specification language

* It's all about the external behavior


sv6: A scalable OS

* What POSIX extensions

 * Maybe say for most programs you could add this flag


Maybe put in a slide with a green triangle

* Our rule says this is possible


Commutative operations can scale

* Title is misleading.  Maybe apply to completely green

 * "It's practical to make commutative operations scale"


Discussion

* Examples


Eddie practice 2013-11-01
-------------------------

Current

* Don't need Y

* Introduce a second workload here.  it's not scalable, so you have to
  go around this again

* And there's a deeper problem.  Transition to next.


Problem

* Happens too late

* Some problems are baked into the design of the system

* For example, ... creat sucks

* No cycle here (just software pipeline)
