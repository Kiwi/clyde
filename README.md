= Clyde update for Pacman 3.4

Here is the readme for my branch of Clyde that should work with
Pacman 3.4.  This is mostly working by now.  Not quite, though.  Test
test program breaks.

= Building example

You don't have to install pacman 3.4 just to test if this works.  You
can install pacman to a non-standard directory.  This is what I did.

== Build pacman

    git clone git://projects.archlinux.org/pacman
    cd pacman
    ./configure --prefix=~/pacmantest
    make test
    make install

This is mostly just to get libalpm.  _libalpm.so_ (and friends) should be
in the _~/pacmantest/lib_ directory now.  The headers are in
_~/pacmantest/include_.

Now you can build clyde like so:

== Build clyde

    git clone -b pacman3.4 git://github.com/juster/clyde.git
    cd clyde
    CPATH=~/pacmantest/include LIBRARY_PATH=~/pacmantest/lib \
      LD_RUN_PATH=~/pacmantest/lib make
    ./test

So on and so forth...

= Authors

Clyde is written by DigitalKiwi and Ghost1227.
Clyde was updated to use pacman 3.4 by juster.
