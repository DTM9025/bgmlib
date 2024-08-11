## How to roughly compile this on Visual Studio 2017

* Make sure you got all submodules:

  ```
  $ git submodule update --init --recursive
  ```

* Convert all other Visual Studio solutions to the current version. With FOX, you only need to build the `fox` and `reswrap`, you can disable all other projects to save time. This fork should contain the `.sln` or the `.vcxproj` files needed for each project to build.

* Make sure to consistently set the same *C/C++ → Code Generation → Runtime Library* option in all projects, because not all of them will come with the same by default.

  Most of them should then compile fine. See the [README](https://github.com/DTM9025/musicroom#touhou-music-room) for the Touhou Music Room for more detailed instructions to build this for that program.

## License

In summary, unless stated otherwise, bgmlib and its source files are licensed under the terms of the GNU General Public License version 3 as published by the Free Software Foundation.

This project uses various third-party libraries, which have their own licenses, compatible with this project's license.

The full license terms are detailed in [Touhou Music Room's `LICENSE.md`](https://github.com/DTM9025/musicroom/blob/master/LICENSE.md)
