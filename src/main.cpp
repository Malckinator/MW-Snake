#define MW_INCLUDE_VULKAN
#include "mwengine.h"
#include <thread>

using namespace mwengine;
using namespace RenderAPI::Vulkan;

void Callback(void* userPtr, Event& event);

struct Snake {
	glm::uvec2 pos;
	Snake* next;

	Snake(glm::uvec2 pos, Snake* next) {
		this->pos = pos;
		this->next = next;
	}

	uint32 GetSize() {
		Snake* ptr = this;
		uint32 count = 0;

		while (ptr != nullptr) {
			count++;
			ptr = ptr->next;
		}

		return count;
	}
};

struct Vertex {
	glm::vec2 position;
};

struct Instance {
	glm::vec3 color;
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

const std::vector<Vertex> vertices = {
	{ { 0.5f, 0.5f } },
	{ { 0.5f, -0.5f } },
	{ { -0.5f, -0.5f } },
	{ { -0.5f, 0.5f } },
};

const std::vector<uint16> indices = {
	0, 1, 2, 2, 3, 0
};

class Application {
public:
	Application() {
		HideConsole();
		SetupGraphics();

		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point frameStart = std::chrono::high_resolution_clock::now();
		const float ticksPerSecond = 5;
		const float targetFps = 60;
		const uint64 nanosecondsPerFrame = (1 / targetFps) * 1'000'000'000.0f;
		float tickAcc = 0.0f;
		float renderAcc = 0.0f;

		while (!toClose) {
			std::chrono::duration timeCh = std::chrono::high_resolution_clock::now() - start;
			start = std::chrono::high_resolution_clock::now();
			uint64 timeNs = timeCh.count();
			float time = timeNs / 1'000'000'000.0f;

			window->Update();

			renderAcc += time;
			while (renderAcc >= 1 / targetFps) {
				std::chrono::duration frameTimeCh = std::chrono::high_resolution_clock::now() - frameStart;
				frameStart = std::chrono::high_resolution_clock::now();
				uint64 frameTimeNs = frameTimeCh.count();
				float frameTime = frameTimeNs / 1'000'000'000.0f;

				if (showFps)
					MW_LOG("FPS: {0}\nFrametime: {1}\n", 1 / frameTime, frameTime);

				Render();

				renderAcc -= 1 / targetFps;
			}

			tickAcc += time;
			while (tickAcc >= 1 / ticksPerSecond && !lost) {
				Tick();

				tickAcc -= 1 / ticksPerSecond;
			}
		}

		instance->WaitUntilIdle();
	}

	~Application() {
		delete graphicsPipeline;
		delete instanceBuffer;
		delete indexBuffer;
		delete vertexBuffer;
		delete fragmentShader;
		delete vertexShader;
		delete commandBuffer;
		delete display;
		delete instance;
		delete window;

		DestroySnake(snake);
	}

	void Tick() {
		glm::ivec2 newPos = snake->pos + moveDir;
		if (newPos.x < 0)
			newPos.x = boardSize.x - 1;
		if (newPos.x >= boardSize.x)
			newPos.x = 0;
		if (newPos.y < 0)
			newPos.y = boardSize.y - 1;
		if (newPos.y >= boardSize.y)
			newPos.y = 0;

		if (CollidesWithSnake(newPos)) {
			lost = true;
			MW_LOG("Lost game.");
			return;
		}

		// create new head
		snake = new Snake(newPos, snake);
		
		if ((glm::uvec2) newPos == apple) {
			apple = GetApplePos();
			return;
		}

		// delete tail
		Snake* last = nullptr;
		Snake* ptr = snake;
		while (true) {
			if (ptr->next == nullptr) {
				delete ptr;
				last->next = nullptr;
				break;
			}

			last = ptr;
			ptr = last->next;
		}

		instance->WaitUntilIdle();
		GenMeshes();
	}

	void SetupGraphics() {
		window = new Window({ 800, 800 }, "Snake", "Snake", this);
		window->SetCallback(Callback);

		snake = new Snake(boardSize / 2u, nullptr);
		apple = GetApplePos();

		instance = new vkInstance(window, "Snake", MW_MAKE_VERSION(0, 0, 1), true);
		display = new vkDisplay(instance, window);
		commandBuffer = new vkCommandBuffer(instance);
		vertexShader = new vkShader(instance, "res/vert.spv");
		fragmentShader = new vkShader(instance, "res/frag.spv");
		vertexBuffer = new vkBuffer(instance, sizeof(Vertex) * vertices.size());
		indexBuffer = new vkBuffer(instance, sizeof(uint16) * indices.size());

		void* data = vertexBuffer->MapMemory(vertexBuffer->GetSize(), 0);
		memcpy(data, vertices.data(), vertexBuffer->GetSize());
		vertexBuffer->UnmapMemory(data);

		data = indexBuffer->MapMemory(indexBuffer->GetSize(), 0);
		memcpy(data, indices.data(), indexBuffer->GetSize());
		indexBuffer->UnmapMemory(data);

		GenMeshes();
	}

	void Render() {
		commandBuffer->StartFrame(display);
		commandBuffer->QueueDraw(graphicsPipeline);
		commandBuffer->EndFrame();
	}

	void GenMeshes() {
		uint32 quads = snake->GetSize() + 1;

		if (instanceBuffer != nullptr) {
			delete instanceBuffer;
		}

		instanceBuffer = new vkBuffer(instance, sizeof(Instance) * quads);
		Instance* data = (Instance*) instanceBuffer->MapMemory(instanceBuffer->GetSize(), 0);

		glm::mat4 view = glm::translate(glm::mat4(1), glm::vec3(0.5, 0.5, 0));
		glm::mat4 proj = glm::ortho<float>(0, boardSize.x, 0, boardSize.y, -1, 1);

		Snake* curSnake = snake;
		for (uint32 i = 0; i < quads - 1; i++) {
			data[i].color = snakeColor;
			data[i].model = glm::translate(glm::mat4(1), glm::vec3(curSnake->pos, 0));
			data[i].view = view;
			data[i].proj = proj;

			curSnake = curSnake->next;
		}

		data[quads - 1].color = appleColor;
		data[quads - 1].model = glm::translate(glm::mat4(1), glm::vec3(apple, 0));
		data[quads - 1].view = view;
		data[quads - 1].proj = proj;

		instanceBuffer->UnmapMemory(data);

		if (graphicsPipeline != nullptr) {
			delete graphicsPipeline;
		}

		graphicsPipeline = new vkGraphicsPipeline(instance);
		graphicsPipeline->vertexBuffer = vertexBuffer;
		graphicsPipeline->vertexSpecification = { SHADER_DATA_TYPE_FLOAT_VEC2 };
		graphicsPipeline->indexBuffer = indexBuffer;
		graphicsPipeline->indexCount = indices.size();
		graphicsPipeline->instanceBuffer = instanceBuffer;
		graphicsPipeline->instanceSpecification = {
			SHADER_DATA_TYPE_FLOAT_VEC3,
			SHADER_DATA_TYPE_FLOAT_VEC4, SHADER_DATA_TYPE_FLOAT_VEC4, SHADER_DATA_TYPE_FLOAT_VEC4, SHADER_DATA_TYPE_FLOAT_VEC4,
			SHADER_DATA_TYPE_FLOAT_VEC4, SHADER_DATA_TYPE_FLOAT_VEC4, SHADER_DATA_TYPE_FLOAT_VEC4, SHADER_DATA_TYPE_FLOAT_VEC4,
			SHADER_DATA_TYPE_FLOAT_VEC4, SHADER_DATA_TYPE_FLOAT_VEC4, SHADER_DATA_TYPE_FLOAT_VEC4, SHADER_DATA_TYPE_FLOAT_VEC4
		};
		graphicsPipeline->instanceCount = quads;
		graphicsPipeline->cullingMode = CULLING_MODE_BACK_BIT;
		graphicsPipeline->imageFormat = display->GetImageFormat();
		graphicsPipeline->vertexShader = { vertexShader, "main" };
		graphicsPipeline->fragmentShader = { fragmentShader, "main" };
		graphicsPipeline->Rebuild();
	}

	glm::uvec2 GetApplePos() {
		std::vector<glm::uvec2> positions;

		for (uint32 x = 0; x < boardSize.x; x++) {
			for (uint32 y = 0; y < boardSize.y; y++) {
				glm::uvec2 pos = glm::uvec2(x, y);

				if (CollidesWithSnake(pos))
					continue;

				positions.push_back(pos);
			}
		}

		srand(std::chrono::high_resolution_clock::now().time_since_epoch().count());
		float index = rand() / (float) RAND_MAX * positions.size();
		return positions[(uint32) index];
	}

	void DestroySnake(Snake* ptr) {
		if (ptr != nullptr) {
			DestroySnake(ptr->next);
			delete ptr;
		}
	}

	bool CollidesWithSnake(glm::uvec2 pos) {
		Snake* ptr = snake;
		while (ptr != nullptr) {
			if (ptr->pos == pos) {
				return true;
			}

			ptr = ptr->next;
		}

		return false;
	}

	const glm::uvec2 boardSize = glm::uvec2(20, 20);
	const glm::vec3 snakeColor = glm::vec3(0, 1, 0);
	const glm::vec3 appleColor = glm::vec3(1, 0, 0);

	bool toClose = false;
	bool lost = false;
	bool showFps = false;
	Window* window;
	vkInstance* instance;
	vkDisplay* display;
	vkCommandBuffer* commandBuffer;
	vkShader* vertexShader;
	vkShader* fragmentShader;
	vkBuffer* vertexBuffer;
	vkBuffer* indexBuffer;
	vkBuffer* instanceBuffer = nullptr;
	vkGraphicsPipeline* graphicsPipeline = nullptr;

	Snake* snake;
	glm::uvec2 apple;
	glm::uvec2 moveDir = glm::uvec2(-1, 0);
};

void Callback(void* userPtr, Event& event) {
	Application* app = (Application*) userPtr;

	switch (event.GetType()) {
	case EventType::WindowClosed:
		app->toClose = true;
		break;
	case EventType::KeyDown:
		if (!((KeyDownEvent*) &event)->IsRepeat()) {
			glm::uvec2 newDir = app->moveDir;

			switch (((KeyDownEvent*) &event)->GetKeycode()) {
			case Keycode::Escape:
				app->toClose = true;
				break;
			case Keycode::C:
				if (IsConsoleVisible())
					HideConsole();
				else
					ShowConsole();
				break;
			case Keycode::Space:
				app->DestroySnake(app->snake);
				app->snake = new Snake(app->boardSize / 2u, nullptr);
				app->apple = app->GetApplePos();
				app->lost = false;
				break;
			case Keycode::F:
				app->showFps = !app->showFps;
				break;
			case Keycode::W:
				newDir = glm::uvec2(0, -1);
				break;
			case Keycode::S:
				newDir = glm::uvec2(0, 1);
				break;
			case Keycode::A:
				newDir = glm::uvec2(-1, 0);
				break;
			case Keycode::D:
				newDir = glm::uvec2(1, 0);
				break;
			}

			if (app->snake->next != nullptr && app->snake->pos + newDir == app->snake->next->pos) {
				return;
			}

			app->moveDir = newDir;
		}
	}
}

int main() {
	Application* app = new Application();
	delete app;
}