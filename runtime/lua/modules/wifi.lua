local native = assert(_G._native_modules, "native modules are unavailable")
return assert(native.wifi, "wifi module is unavailable on this board/build")
