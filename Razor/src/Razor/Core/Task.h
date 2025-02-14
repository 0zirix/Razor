#pragma once

#include "Core.h"
#include "Razor/Types/Variant.h"
#include "Razor/Scene/Node.h"

namespace Razor {

	typedef std::function<void(Node*)> TaskFinished;
	typedef std::function<void(Node*, TaskFinished, Variant)> TaskCallback;

	typedef struct {
		Node* result;
		TaskCallback fn;
		TaskFinished tf;
		Variant opts;
		const char* name;
		unsigned int priority = 0;
	} TaskData;

	class RAZOR_API Task
	{
	public:
		Task(const TaskData& data) :
		result(data.result),
		tf(std::bind(data.tf, data.result)),
		fn(std::bind(data.fn, data.result, data.tf, data.opts)),
		opts(data.opts),
		name(data.name),
		priority(data.priority)
			{}

		void execute() {
			run(result, fn, opts, tf);
		}

		~Task() {

		}

		inline std::string& getName() { return name; }
		inline void setName(const std::string& name) { this->name = name; }
		inline unsigned int getPriority() { return priority; }
		inline void setPriority(unsigned int priority) { this->priority = priority; }
 
		void run(Node* result, TaskCallback fn, Variant opts, TaskFinished tf) {
			fn(result, tf, opts);
		}

		inline bool operator < (const Task& task) const {
			return task.priority > priority;
		}

		Node* result;
		Variant opts;
		TaskCallback fn;
		std::function<void(Node*)> tf;

	private:
		std::string name;
		unsigned int priority;
	};
}

