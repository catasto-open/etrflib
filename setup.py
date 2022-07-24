from distutils.core import setup, Extension


dacsagb_module = Extension('_dacsagb',
    sources=['dacsagb_wrap.cxx', 'dacsagb.cpp'],
)
setup (name = 'dacsagb',
    version = '0.1',
    author = "SWIG Docs",
    description = """Simple swig dacsagb from docs""",
    ext_modules = [dacsagb_module],
    py_modules = ["dacsagb"],
)