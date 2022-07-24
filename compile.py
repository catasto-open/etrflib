import os
import cffi


ffi = cffi.FFI()

with open(os.path.join(os.path.dirname(__file__), "dacsagb.h"), encoding='latin-1') as f:
    ffi.cdef(f.read())

ffi.set_source("_dacsagb",
    '#include "dacsagb.h"',
    libraries=["dacsagb"],
    library_dirs=[os.path.dirname(__file__),],
)

ffi.compile()
