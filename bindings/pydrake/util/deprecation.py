"""Provides deprecation warnings and utilities for triggering warnings.

By default, this sets all `DrakeDeprecationWarnings` to be shown `"once"`,
which overrides any `-W` command-line arguments. To change this behavior, you
can do something like:

>>> import warnings
>>> from pydrake.util.deprecation import DrakeDeprecationWarning
>>> warnings.simplefilter("always", DrakeDeprecationWarning)

If you would like to disable all Drake-related warnings, you may use the
`"ignore"` action for `warnings.simplefilter`.
"""

import sys
import traceback
from types import ModuleType
import warnings

# TODO(eric.cousineau): Make autocomplete ignore `ModuleShim` attributes
# (e.g. `install`).


class ModuleShim(object):
    """Provides a shim for automatically resolving extra variables.

    This can be used to deprecate import alias in modules to simplify
    dependencies.

    See Also:
        https://stackoverflow.com/a/7668273/7829525
    """

    def __init__(self, orig_module, handler):
        assert hasattr(orig_module, "__all__"), (
            "Please define `__all__` for this module.")
        assert isinstance(orig_module, ModuleType), (
            "{} must be a module".format(orig_module))
        # https://stackoverflow.com/a/16237698/7829525
        object.__setattr__(self, '_orig_module', orig_module)
        object.__setattr__(self, '_handler', handler)
        object.__setattr__(self, '__doc__', orig_module.__doc__)

    def __getattr__(self, name):
        # Use the original module if possible.
        m = self._orig_module
        if hasattr(m, name):
            return getattr(m, name)
        else:
            # Otherwise, use the handler, and store the result.
            try:
                value = self._handler(name)
            except AttributeError as e:
                if e.message:
                    raise e
                else:
                    raise AttributeError(
                        "'module' object has no attribute '{}'".format(name))
            setattr(m, name, value)
            return value

    def __setattr__(self, name, value):
        # Redirect writes to the original module.
        setattr(self._orig_module, name, value)

    def __delattr__(self, name):
        # Redirect deletions to the original module.
        delattr(self._orig_module, name)

    def __repr__(self):
        return repr(self._orig_module)

    def __dir__(self):
        # Implemented to provide a useful subset of completions to
        # `rlcompleter`.
        return self._orig_module.__all__

    @classmethod
    def _install(cls, name, handler):
        """Hook into module's attribute accessors and mutators.

        Args:
            name: Module name. Generally should be __name__.
            handler: Function of the form `handler(var)`, where `var` is the
                variable name.

        Note:
            This is private such that `install` does not pollute completion
            candidations provided by `rlcompleter` when it iterates through
            `__bases__`.
        """
        old_module = sys.modules[name]
        new_module = cls(old_module, handler)
        sys.modules[name] = new_module


class DrakeDeprecationWarning(DeprecationWarning):
    """Extends `DeprecationWarning` to permit Drake-specific warnings to
    be filtered by default, without having side effects on other libraries.
    """
    addendum = ("\n    Please see `help(pydrake.util.deprecation)` " +
                "for more information.")

    def __init__(self, message, *args):
        extra_message = message + DrakeDeprecationWarning.addendum
        DeprecationWarning.__init__(self, extra_message, *args)


def _warn_deprecated(message, stacklevel=2):
    # Logs a deprecation warning message.  Also used by `deprecation_pybind.h`
    # in addition to this file.
    warnings.warn(
        message, category=DrakeDeprecationWarning, stacklevel=stacklevel)


class _DeprecatedDescriptor(object):
    """Wraps a descriptor to warn that it is deprecated any time it is
    acccessed.
    """

    def __init__(self, original, message):
        assert hasattr(original, '__get__'), "`original` must be a descriptor"
        self._original = original
        self.__doc__ = self._original.__doc__
        self._message = message

    def _warn(self):
        _warn_deprecated(self._message, stacklevel=4)

    def __get__(self, obj, objtype):
        self._warn()
        return self._original.__get__(obj, objtype)

    def __set__(self, obj, value):
        self._warn()
        self._original.__set__(obj, value)

    def __delete__(self, obj):
        self._warn()
        self._original.__delete__(obj)


def deprecated(message):
    """Decorator that deprecates a member of a class based on access.

    Args:
        message: Warning message when member is accessed.

    Note:
        This differs from other implementations in that it warns on access,
        not when the method is called. For other methods, see the examples in
        https://stackoverflow.com/a/40301488/7829525.

    Use `ModuleShim` for deprecating variables in a module.
    """
    def wrapped(original):
        return _DeprecatedDescriptor(original, message)

    return wrapped


def install_numpy_warning_filters(force=False):
    """Install warnings filters specific to NumPy."""
    global installed_numpy_warning_filters
    if installed_numpy_warning_filters and not force:
        return
    installed_numpy_warning_filters = True
    # Warnings specific to comparison with `dtype=object` should be raised to
    # errors (#8315, #8491). Without them, NumPy will return effectively
    # garbage values (e.g. comparison based on object ID): either a scalar bool
    # or an array of bools (based on what objects are present and the NumPy
    # version).
    # N.B. Using a `module=` regex filter does not work, as the warning is
    # raised from C code, and thus inherits the calling module, which may not
    # be "numpy\..*" (numpy/numpy#10861).
    warnings.filterwarnings(
        "error", category=DeprecationWarning, message="numpy equal will not")
    warnings.filterwarnings(
        "error", category=DeprecationWarning,
        message="elementwise == comparison failed")


warnings.simplefilter('once', DrakeDeprecationWarning)
installed_numpy_warning_filters = False
