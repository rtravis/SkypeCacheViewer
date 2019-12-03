# Skype Cache Viewer

Display contact information and messages extracted from a Skype cache
LevelDB database.

## Building

A POSIX compliant system (such as GNU/Linux) is required and
additionally the following libraries are needed: pthread, crypto
(OpenSSL) and leveldb.

The command line executable can be built using CMake:

    mkdir build && cd build
    cmake .. && make

Then you can run the executable like this:

    ./SkypeCacheViewer ${HOME}/.config/skypeforlinux/IndexedDB/file__0.indexeddb.leveldb

## License

Unless otherwise specified a BSD 2-Clause License applies. Code is
copyrighted as indicated in the file header comments. The LevelDB
custom comparator code has been taken from the Chromium project.
