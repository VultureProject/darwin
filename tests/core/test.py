import core.base as base
import core.redis as redis


def run():
    print("Core Results:")

    base.run()
    redis.run()

    print()
    print()