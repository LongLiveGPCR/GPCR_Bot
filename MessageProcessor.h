#pragma once
#include <string>
#include <regex>
#include <vector>
#include <functional>
namespace MyStd
{
	template<typename ...T>
	class MessageProcessor
	{
	public:
		using ProcessorFunction = std::function<void(const std::smatch&, T...)>;
		class Processor
		{
			friend class MessageProcessor;
		public:
			std::regex regex;
			ProcessorFunction function;
			std::string regexStr;
			Processor(const std::string& reg, const ProcessorFunction& func);
		private:
			bool isDeleted;
		};
		MessageProcessor()noexcept;
		void AddMatchProcessor(const std::string& regex, const ProcessorFunction& func);
		void RemoveMatchProcessor(const size_t& index);
		size_t RemoveMatchProcessor(const std::string& reg);
		void AddSearchProcessor(const std::string& regex, const ProcessorFunction& func);
		void RemoveSearchProcessor(const size_t& index);
		size_t RemoveSearchProcessor(const std::string& reg);
		size_t ProcessMessage(const std::string& message, T... args);
		const std::vector<Processor>& GetMatchProcessorsList()const noexcept;
		const std::vector<Processor>& GetSearchProcessorsList()const noexcept;
	private:
		std::vector<Processor> mMatchProcessors;
		std::vector<Processor> mSearchProcessors;
		std::vector<Processor> mPendingMatchProcessors;
		std::vector<Processor> mPendingSearchProcessors;
		bool mIsProcessingMessage;//为了防止在遍历容器处理消息的过程中添加或删除Processor
	};

	template<typename ...T>
	MessageProcessor<T...>::MessageProcessor() noexcept :mIsProcessingMessage(false)
	{
	}

	template<typename ...T>
	void MessageProcessor<T...>::AddMatchProcessor(const std::string& regex, const ProcessorFunction& func)
	{
		if (mIsProcessingMessage)
			mPendingMatchProcessors.push_back(Processor(regex, func));
		else
			mMatchProcessors.push_back(Processor(regex, func));
	}

	template<typename ...T>
	void MessageProcessor<T...>::RemoveMatchProcessor(const size_t& index)
	{
		if (mIsProcessingMessage)
			mMatchProcessors.at(index).isDeleted = true;
		else
			mMatchProcessors.erase(mMatchProcessors.begin() + index);
	}

	template<typename ...T>
	size_t MessageProcessor<T...>::RemoveMatchProcessor(const std::string& reg)
	{
		auto iter = std::remove_if(mMatchProcessors.begin(), mMatchProcessors.end(), [reg](const Processor& p) {
			return p.regexStr == reg;
			});
		const size_t res = mMatchProcessors.end() - iter;
		if (iter != mMatchProcessors.end())
			mMatchProcessors.erase(iter, mMatchProcessors.end());
		return res;
	}

	template<typename ...T>
	void MessageProcessor<T...>::AddSearchProcessor(const std::string& regex, const ProcessorFunction& func)
	{
		if (mIsProcessingMessage)
			mPendingSearchProcessors.push_back(Processor(regex, func));
		else
			mSearchProcessors.push_back(Processor(regex, func));
	}

	template<typename ...T>
	void MessageProcessor<T...>::RemoveSearchProcessor(const size_t& index)
	{
		if (mIsProcessingMessage)
			mSearchProcessors.at(index).isDeleted = true;
		else
			mSearchProcessors.erase(mMatchProcessors.begin() + index);
	}

	template<typename ...T>
	size_t MessageProcessor<T...>::RemoveSearchProcessor(const std::string& reg)
	{
		auto iter = std::remove_if(mSearchProcessors.begin(), mSearchProcessors.end(), [reg](const Processor& p) {
			return p.regexStr == reg;
			});
		const size_t res = mSearchProcessors.end() - iter;
		if (iter != mSearchProcessors.end())
			mSearchProcessors.erase(iter, mSearchProcessors.end());
		return res;
	}

	template<typename ...T>
	size_t MessageProcessor<T...>::ProcessMessage(const std::string& message, T... args)
	{
		std::smatch m;
		size_t count = 0;
		mIsProcessingMessage = true;
		for (const auto& iter : mMatchProcessors)
		{
			if (std::regex_match(message, m, iter.regex))
			{
				++count;
				iter.function(m, std::forward<T>(args)...);
			}
		}
		for (const auto& iter : mSearchProcessors)
		{
			if (std::regex_search(message, m, iter.regex))
			{
				++count;
				iter.function(m, std::forward<T>(args)...);
			}
		}
		mIsProcessingMessage = false;
		while (!mPendingMatchProcessors.empty())//将等待中的MatchProcessor添加到正式容器中
		{
			mMatchProcessors.push_back(mPendingMatchProcessors.back());
			mPendingMatchProcessors.pop_back();
		}
		while (!mPendingSearchProcessors.empty())//将等待中的SearchProcessor添加到正式容器中
		{
			mSearchProcessors.push_back(mPendingSearchProcessors.back());
			mPendingSearchProcessors.pop_back();
		}
		mMatchProcessors.erase(std::remove_if(mMatchProcessors.begin(), mMatchProcessors.end(), [](const Processor& p) {
			return p.isDeleted;
			}), mMatchProcessors.end());//删除等待被删除的MatchPrecessor
		mSearchProcessors.erase(std::remove_if(mSearchProcessors.begin(), mSearchProcessors.end(), [](const Processor& p) {
			return p.isDeleted;
			}), mSearchProcessors.end());//删除等待被删除的SearchPrecessor
		return count;
	}

	template<typename ...T>
	const std::vector<typename MessageProcessor<T...>::Processor>& MessageProcessor<T...>::GetMatchProcessorsList() const noexcept
	{
		return mMatchProcessors;
	}

	template<typename ...T>
	const std::vector<typename MessageProcessor<T...>::Processor>& MessageProcessor<T...>::GetSearchProcessorsList() const noexcept
	{
		return mSearchProcessors;
	}

	template<typename ...T>
	MessageProcessor<T...>::Processor::Processor(const std::string& reg, const ProcessorFunction& func) :regex(reg), regexStr(reg), function(func), isDeleted(false)
	{
	}
};