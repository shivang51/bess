class Component:
    def __init__(self, name):
        self.name = name
        self.type = "Generic"

    def update(self, delta_time):
        print("[BessPlugin][Update]", delta_time);

