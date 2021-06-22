from .Container import Container


class FlowContainer(Container):
    def __init__(self, name, engine, vols, network):
        super().__init__(name, engine, vols, network)
        self.flow = None

    def get_flow(self):
        return self.flow

    def set_flow(self, flow):
        self.flow = flow
