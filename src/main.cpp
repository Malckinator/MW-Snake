#define MW_INCLUDE_VULKAN
#include "mwengine.h"

using namespace mwengine;

bool toClose = false;

void Callback(Event& event) {
	switch (event.GetType()) {
	case EventType::WindowClosed:
		toClose = true;
		break;
	}
}

int main() {
	Window window = Window({ 640, 480 }, "Snake", "Snake");
	window.SetCallback(Callback);

	while (!toClose) {
		window.Update();
	}
}