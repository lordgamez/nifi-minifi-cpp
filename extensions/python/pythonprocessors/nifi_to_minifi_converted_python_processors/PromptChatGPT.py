import codecs
from nifi_python_processors import PromptChatGPT

class ReadCallback:
    def process(self, input_stream):
        self.content = codecs.getreader('utf-8')(input_stream).read()
        return len(self.content)

def describe(processor):
    processor.setDescription("Moves content of flow file to JSON file under 'content' key")


def onInitialize(processor):
    processor.setSupportsDynamicProperties()


def onTrigger(context, session):
    flow_file = session.get()
    if flow_file is not None:
        read_callback = ReadCallback()
        session.read(flow_file, read_callback)

