
class Logger:
    def __init__(self, name):
        self.name = name

    def log(self, message):
        print(f"[Plugin][{self.name}] {message}")
