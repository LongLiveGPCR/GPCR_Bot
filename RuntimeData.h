#pragma once
#include<string>
#include<unordered_map>
namespace MyStd
{
	class RuntimeDataManager
	{
	private:
		class RuntimeDataBase
		{
			friend class RuntimeDataManager;
		protected:
			virtual void ReleaseData() = 0;
			std::string id;
		public:
			RuntimeDataBase() {}
			RuntimeDataBase(const std::string& _id) :id(_id) {}
			RuntimeDataBase(const RuntimeDataBase& d) :id(d.id) {}
			std::string GetID()const
			{
				return id;
			}
		};
	public:
		std::unordered_map<std::string, RuntimeDataBase*>data;
		template<typename T>
		class RuntimeData :public RuntimeDataBase
		{
			friend class RuntimeDataManager;
		private:
			template<typename... Args>
			RuntimeData(const std::string& _id, Args&&... args) :RuntimeDataBase(_id), value(new T(std::forward<Args>(args)...)) {}
			template<typename... Args>
			RuntimeData() : value(nullptr) {}
			T* value;
		protected:
			virtual void ReleaseData()
			{
				delete value;
			}
		public:
			RuntimeData(const RuntimeData& data) :RuntimeDataBase(data), value(data.value) {}
			operator T*()const
			{
				return value;
			}
			T& operator* ()const
			{
				return *value;
			}
			T* operator-> ()const
			{
				return value;
			}
		};
		template<typename T>
		RuntimeData<T> GetData(const std::string& id)
		{
			if (auto iter = data.find(id); iter != data.end())
			{
				RuntimeData<T>* t = dynamic_cast<RuntimeData<T>*>(iter->second);
				if (t != nullptr)
					return *t;
				return RuntimeData<T>();
			}
			return RuntimeData<T>();
		}
		bool FindData(const std::string& id)
		{
			return data.find(id) != data.end();
		}
		template<typename T, typename...Args>
		RuntimeData<T> AddData(const std::string& id, Args&& ...args)
		{
			RemoveData(id);
			data.emplace(id, new RuntimeData<T>(id, std::forward<Args>(args)...));
			return GetData<T>(id);
		}
		bool RemoveData(const std::string& id)
		{
			if (auto iter = data.find(id); iter != data.end())
			{
				iter->second->ReleaseData();
				delete iter->second;
				data.erase(iter);
				return true;
			}
			return false;
		}
		template<typename T>
		bool RemoveData(const RuntimeData<T>& iter)
		{
			return data.erase(iter.id);
		}
		~RuntimeDataManager()
		{
			for (const auto& i : data)
			{
				i.second->ReleaseData();
				delete i.second;
			}
		}
	};
}