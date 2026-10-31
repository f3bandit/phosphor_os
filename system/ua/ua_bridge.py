#!/usr/bin/env python3
# Phosphor: a bridge to Update All's own settings code (its update_all.pyz on the SD card), so Phosphor's
# Settings > Update All shows Update All's whole list, as Update All itself loads it, and saves changes the way
# Update All's own menu does. Part of phosphor_os. Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0.
#
#   ua_bridge.py dump            the menus, entries, descriptions and current values, as JSON
# Reading only: Update All's start-up migrations (which write) are not run here.
import sys, os, json, io, contextlib

PYZ = os.environ.get('UA_PYZ', '/media/fat/Scripts/.config/update_all/update_all.pyz')
sys.path.insert(0, PYZ)


class Anything:
    """Stands in for Update All's curses screen: every attribute and call returns another one."""
    def __getattr__(self, name):
        return Anything()

    def __call__(self, *a, **k):
        return Anything()

    def __iter__(self):
        return iter([Anything(), Anything(), Anything()])

    def __bool__(self):
        return False


class NoScreen:
    def initialize_screen(self, *a, **k):
        return Anything(), Anything(), Anything()

    def __getattr__(self, name):
        return Anything()


class FakeUi:
    def __init__(self):
        self.values = {}
        self.effects = {}
        self.formatters = {}

    def get_value(self, key):
        return self.values.get(key, '')

    def set_value(self, key, value):
        value = value if isinstance(value, str) else str(value)
        changed = self.values.get(key) != value
        self.values[key] = value
        return changed

    def add_custom_effects(self, effects):
        self.effects.update(effects)

    def add_custom_formatters(self, formatters):
        self.formatters.update(formatters)


def build():
    from update_all.logger import PrintLogger
    from update_all.other import GenericProvider
    from update_all.update_all_service import UpdateAllServiceFactory
    logger = PrintLogger()
    from update_all.main import read_env
    try:
        from commit import default_commit
    except ImportError:
        from update_all.constants import DEFAULT_COMMIT as default_commit
    import time
    env = read_env(default_commit, time.monotonic())
    service = UpdateAllServiceFactory(logger, local_repository_provider=GenericProvider()).create(env)
    return service


def load(service):
    from update_all.config import Config
    es = service._environment_setup
    config = Config()
    es._config_provider.initialize(config)
    es._config_reader.fill_config_with_environment(config)
    store = es._local_repository.load_store()
    es._store_provider.initialize(store)
    ini = es._config_reader.read_downloader_ini()
    es._config_reader.fill_config_with_mister_section(config, ini)
    es._config_reader.fill_config_with_local_store(config, store)
    es._config_reader.fill_config_with_database_sections(config, ini)
    return config


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'dump'
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        service = build()
        load(service)
        screen = service._settings_screen
        screen._settings_screen_printer = NoScreen()
        ui = FakeUi()
        screen.initialize_ui(ui)
        model = screen._model
    if cmd == 'dump':
        json.dump({'ok': True, 'values': ui.values, 'items': model.get('items', {}),
                   'formatters': list(ui.formatters)}, sys.stdout)
        return 0
    json.dump({'ok': False, 'error': 'unknown command'}, sys.stdout)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as e:
        import traceback
        json.dump({'ok': False, 'error': str(e), 'trace': traceback.format_exc()[-2000:]}, sys.stdout)
        sys.exit(1)
