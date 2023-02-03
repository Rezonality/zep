# Clip Library
*Copyright (C) 2015-2019 David Capello*

[![Build Status](https://travis-ci.org/dacap/clip.svg)](https://travis-ci.org/dacap/clip)
[![Build status](https://ci.appveyor.com/api/projects/status/iyory4t5oop7ssrs?svg=true)](https://ci.appveyor.com/project/dacap/clip)
[![MIT Licensed](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.txt)

Library to copy/retrieve content to/from the clipboard/pasteboard.

## Features

Available features on Windows, macOS, and Linux (X11):

* Copy/paste UTF-8 text.
* Copy/paste user-defined data.
* Copy/paste RGB/RGBA images. This library use non-premultiplied alpha RGB values.

## Example

```cpp
#include "clip.h"
#include <iostream>

int main() {
  clip::set_text("Hello World");

  std::string value;
  clip::get_text(value);
  std::cout << value << "\n";
}
```

## User-defined clipboard formats

```cpp
#include "clip.h"

int main() {
  clip::format my_format =
    clip::register_format("com.appname.FormatName");

  int value = 32;

  clip::lock l;
  l.clear();
  l.set_data(clip::text_format(), "Alternative text for value 32");
  l.set_data(my_format, &value, sizeof(int));
}
```

## Platform specific details

* **Windows**:
  - [Limited number of clipboard formats on Windows](http://blogs.msdn.com/b/oldnewthing/archive/2015/03/19/10601208.aspx)
* **Linux**:
  - To be able to copy/paste on Linux you need `libx11-dev`/`libX11-devel` package.
  - To copy/paste images you will need `libpng-dev`/`libpng-devel` package.

## Who is using this library?

[Check the wiki](https://github.com/dacap/clip/wiki#who-is-using-clip)
to know what projects are using the `clip` library.
