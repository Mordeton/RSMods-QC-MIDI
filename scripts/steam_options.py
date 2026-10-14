"""Edit one game's Steam launch options without reserializing unrelated VDF data."""
import re


TOKEN = re.compile(r'//[^\n]*|"(?:\\.|[^"\\])*"|[{}]')


def unquote(token):
    return re.sub(r'\\(["\\])', r'\1', token[1:-1])


def quote(value):
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def entries(text):
    tokens = [m for m in TOKEN.finditer(text) if not m.group().startswith('//')]
    index = 0

    def parse(nested=False):
        nonlocal index
        result = {}
        while index < len(tokens):
            key = tokens[index]
            index += 1
            if key.group() == '}':
                if not nested:
                    raise ValueError('Unexpected VDF closing brace')
                return result, key.start()
            if not key.group().startswith('"') or index >= len(tokens):
                raise ValueError('Invalid VDF key')
            value = tokens[index]
            index += 1
            name = unquote(key.group()).casefold()
            if name in result:
                raise ValueError('Ambiguous duplicate VDF key: ' + name)
            if value.group() == '{':
                children, close = parse(True)
                result[name] = (children, value.start(), close)
            elif value.group().startswith('"'):
                result[name] = (unquote(value.group()), value.start(), value.end())
            else:
                raise ValueError('Invalid VDF value')
        if nested:
            raise ValueError('Unclosed VDF object')
        return result, len(text)

    return parse()[0]


def game_object(text, appid='221680'):
    obj = entries(text)
    for key in ['userlocalconfigstore', 'software', 'valve', 'steam', 'apps', appid]:
        value, start, end = obj[key]
        if not isinstance(value, dict):
            raise ValueError('Unexpected Steam application configuration')
        obj = value
    return obj, end


def get_options(text, appid='221680'):
    obj, _ = game_object(text, appid)
    return obj.get('launchoptions', ('', 0, 0))[0]


def set_options(text, options, appid='221680'):
    obj, close = game_object(text, appid)
    if 'launchoptions' in obj:
        _, start, end = obj['launchoptions']
        return text[:start] + quote(options) + text[end:]
    return text[:close] + '\t"LaunchOptions"\t\t' + quote(options) + '\n\t\t\t\t\t' + text[close:]
