import logging

class CustomAdapter(logging.LoggerAdapter):
    def process(self, msg, kwargs):
        return '%s: %s' % (self.extra['test_name'], msg), kwargs