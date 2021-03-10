import operator as op

class ApproxDict(object):
    def __init__(self, iterable=(), float_eq=op.eq):
        self._float_eq = float_eq
        self._dict = dict(iterable)

    def __getitem__(self, key):
        return self._dict[key]

    def __setitem__(self, key, val):
        self._dict[key] = val

    def __iter__(self):
        return iter(self._dict)

    def __len__(self):
        return len(self._dict)

    def __contains__(self, key):
        return key in self._dict

    def __str__(self):
        return self._dict.__str__()

    def __repr__(self):
        return self._dict.__repr__()

    def __eq__(self, other):
        def compare(a, b):
            if isinstance(a, float) and isinstance(b, float):
                return self._float_eq(a, b)
            else:
                return a == b
        try:
            if len(self) != len(other):
                return False
            for key in self:
                if not compare(self[key], other[key]):
                    return False
            return True
        except Exception:
            return False

    def __getattr__(self, attr):
        attr_val = getattr(self._dict, attr)
        if callable(attr_val):
            def wrapper(*args, **kwargs):
                result = attr_val(*args, **kwargs)
                if isinstance(result, dict):
                    return ApproxDict(result, self._float_eq)
                return result
            return wrapper
        return attr_val