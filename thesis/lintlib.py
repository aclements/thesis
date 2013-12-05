__all__ = ["Text"]

import re

def textLines(text):
    pos = lineno = 0
    while pos < len(text):
        lineno += 1
        next = text.find("\n", pos)
        if next == -1:
            next = len(text)
        yield lineno, pos, next
        pos = next + 1

class Text(object):
    def __init__(self, path, _text = None, _root = None):
        self.__path = path
        self.__text = _text or file(path).read()
        self.__root = _root or self
        if _root == None:
            self.__matches = []
            self.__hasErrors = False

    def mark(self, msg, regex, flags = 0, err = True):
        regex = re.compile(regex, flags | re.M)
        if regex.groups > 1:
            raise ValueError("Regex has >1 groups")
        hasGroup = (regex.groups > 0)
        for match in regex.finditer(self.__text):
            span = match.span(1) if hasGroup else match.span()
            self.__root.__matches.append((match, span, msg, err))
            if err:
                self.__root.__hasErrors = True

    def warn(self, msg, regex, flags = 0):
        self.mark(msg, regex, flags, False)

    def hasErrors(self):
        return self.__hasErrors

    def ignore(self, regex, flags = 0):
        regex = re.compile(regex, flags | re.M)
        if regex.groups > 1:
            raise ValueError("Regex has >1 groups")
        hasGroup = (regex.groups > 0)

        text = self.__text
        for m in regex.finditer(text):
            l, r = m.span(1) if hasGroup else m.span()
            text = "%s%s%s" % (text[:l], " " * (r - l), text[r:])
        return Text(self.__path, text, self.__root)

    def showMarks(self):
        spans = list(sorted(self.__matches, key = lambda (_1, span, _2, _3): span[0]))

        text = self.__text
        for lineno, start, end in textLines(text):
            while spans and spans[0][1][0] < end:
                _, (l, r), msg, err = spans.pop(0)
                out = "\x1b[35m%s\x1b[36m:" % self.__path
                out += "\x1b[32m%d\x1b[36m:\x1b[m " % lineno
                out += "lint %s: " % ("error" if err else "warning")
                out += "\x1b[4mPlease, " + msg + "\x1b[m"
                print out
                out = ("  " + text[start:l] + "\x1b[%sm" % ("01;31" if err else "33") +
                       text[l:r] + "\x1b[m" +
                       text[r:end])
                print out
