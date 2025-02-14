#pragma once

#include "Razor/Core/Core.h"
#include "Editor/EditorComponent.h"
#include "Razor/Filesystem/DirectoryWatcher.h"
#include "Razor/Filesystem/FileWatcher.h"
#include "Razor/Core/TasksManager.h"
#include "FileBrowser.h"
#
namespace Razor {

	class TexturesManager;
	class AssimpImporter;

	class RAZOR_API AssetsManager : public EditorComponent
	{
	public:
		AssetsManager(Editor* editor);
		~AssetsManager();

		void watch();
		void render(float delta) override;
		static void import(Node* result, TaskFinished tf, Variant opts);
		inline std::shared_ptr<FileBrowser> getFileBrowser() { return fileBrowser; }
		inline std::shared_ptr<FileWatcher> getFileWatcher() { return fileWatcher; }

		enum Type { None, Model, Image, Audio, Video, Script };
		typedef std::map<Type, std::vector<const char*>> ExtsMap;

		inline static Type getExt(const std::string& str)
		{
			ExtsMap::iterator it = AssetsManager::exts.begin();

			for (; it != AssetsManager::exts.end(); ++it)
			{
				auto item = std::find(it->second.begin(), it->second.end(), str);

				if (item != it->second.end())
					return it->first;
			}

			return Type::None;
		}

		static TexturesManager* texturesManager;
		static std::unique_ptr<AssimpImporter> importer;

	private:
		static TaskFinished import_callback;
		static bool show_import_modal;
		static std::string current_import_path;
		static std::map<Type, std::vector<const char*>> exts;
		static std::map<Type, std::vector<const char*>> internal_exts;
		std::shared_ptr<DirectoryWatcher> directoryWatcher;
		std::shared_ptr<FileWatcher> fileWatcher;
		std::shared_ptr<FileBrowser> fileBrowser;
	};

}

