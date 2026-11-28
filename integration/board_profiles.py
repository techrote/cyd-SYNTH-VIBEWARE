import configparser

PROFILES = (
    ('instruments/phasespace/firmware', 'cyd-phasespace', '6.10.0'),
    ('instruments/chronogrid/firmware', 'cyd-chronogrid', '6.10.0'),
    ('instruments/xenobio/firmware', 'cyd-xenobio', '6.10.0'),
    ('instruments/fm1-ports/xenoresonator', 'cyd-port-xeno', '7.1.3'),
    ('instruments/fm1-ports/chronomorph', 'cyd-port-chrono', '7.1.3'),
    ('instruments/fm1-ports/ecophon', 'cyd-port-ecophon', '7.1.3'),
    ('instruments/fm1-ports/ecophon', 'cyd-port-ecophon-timer', '7.1.3'),
)


def plan(source):
    result = []
    for directory, profile, platform in PROFILES:
        config = configparser.ConfigParser(interpolation=None)
        with (source / directory / 'platformio.ini').open(encoding='utf-8') as f:
            config.read_file(f)
        default = profile
        section = config['env:' + profile]
        if profile == 'cyd-port-ecophon-timer':
            default = 'cyd-port-ecophon'
            # Only this reviewed inherited alternate is admissible. Do not
            # accidentally accept an override of board, platform or partition.
            if dict(section) != {'extends': 'env:' + default,
                                 'build_flags': '${env:cyd-port-ecophon.build_flags} -DECO_TIMER_DAC'}:
                raise ValueError('unexpected timer override: ' + profile)
            section = config['env:' + default]
        expected = {'platform': 'espressif32 @ ' + platform, 'board': 'esp32dev',
                    'framework': 'arduino', 'board_build.partitions': 'huge_app.csv'}
        if config['platformio']['default_envs'] != default or any(section.get(k) != v for k, v in expected.items()):
            raise ValueError('unexpected profile configuration: ' + profile)
        result.append(dict(directory=directory, profile=profile, **expected))
    return result
