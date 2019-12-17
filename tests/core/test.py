import core.base as base
import core.redis as redis
import core.alert as alert


def run():
    print("Core Results:")

    base.run()
    redis.run()
    alert.run()

    print()
    print()