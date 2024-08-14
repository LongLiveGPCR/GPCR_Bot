#pragma once
#include <mysql.h>
#include <string>
#include <ios>
#include <optional>
#include <chrono>
#include <sstream>
#include <format>
namespace MyStd
{
	bool IgnoreCaseCompare(const std::string& a, const std::string& b)noexcept
	{
		if (a.size() != b.size())
			return false;
		for (size_t i = 0; i < a.size(); i++)
		{
			if (tolower(a[i]) != tolower(b[i]))
				return false;
		}
		return true;
	};
	namespace MySQL
	{
		class MySQLException :public std::ios_base::failure
		{
		public:
			using std::ios_base::failure::failure;
		};
		class MySQLConnectException :public MySQLException
		{
		public:
			using MySQLException::MySQLException;
		};
		class MySQLExecuteException :public MySQLException
		{
		public:
			using MySQLException::MySQLException;
		};
		class MySQLRuntimeException :public MySQLException
		{
		public:
			using MySQLException::MySQLException;
		};
		using DateTime = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;
		using Date = std::chrono::time_point<std::chrono::system_clock, std::chrono::days>;
		class Value
		{
		public:
			Value(const std::string& s) :data(s) {}
			operator bool()const
			{
				return std::stoul(data);
			}
			operator int8_t()const
			{
				return std::stoi(data);
			}
			operator uint8_t()const
			{
				return std::stoul(data);
			}
			operator short()const
			{
				return std::stoi(data);
			}
			operator unsigned short()const
			{
				return std::stoul(data);
			}
			operator int()const
			{
				return std::stoi(data);
			}
			operator unsigned int()const
			{
				return std::stoul(data);
			}
			operator long()const
			{
				return std::stol(data);
			}
			operator unsigned long()const
			{
				return std::stoul(data);
			}
			operator long long()const
			{
				return std::stoll(data);
			}
			operator unsigned long long()const
			{
				return std::stoull(data);
			}
			operator double()const
			{
				return std::stod(data);
			}
			operator long double()const
			{
				return std::stold(data);
			}
			operator DateTime()const
			{
				DateTime res;
				std::istringstream iss(data);
				iss >> std::chrono::parse("%F %T", res);
				return res;
			}
			operator Date()const
			{
				Date res;
				std::istringstream iss(data);
				iss >> std::chrono::parse("%F", res);
				return res;
			}
			operator std::string()const
			{
				return data;
			}
		private:
			std::string data;
		};
		class Row
		{
			friend class QueryResult;
		public:
			Value operator[](size_t index)const
			{
				return data[index];
			}
			const std::vector<Value>& GetData()const noexcept
			{
				return data;
			}
		private:
			std::vector<Value> data;
		};
		class QueryResult
		{
		public:
			QueryResult(MYSQL_RES* result);
			QueryResult(const QueryResult& result) = default;
			QueryResult(QueryResult&& result) = default;
			const std::vector<Row>& GetRows()const noexcept;
			const std::vector<std::string>& GetFields()const noexcept;
			bool IsEmpty()const noexcept;
		private:
			std::vector<Row> rows;
			std::vector<std::string> fields;
		};
		class DataBase
		{
		public:
			DataBase(const std::string& user_name, const std::string& password, const std::string& database, const std::string& characters, const std::string& host = "localhost", const unsigned int port = 3306, const char* unix_socket = nullptr, const unsigned long client_flag = 0);
			std::optional<QueryResult> Execute(const std::string& str);
			std::string GetDataBaseName()const noexcept;
			std::string EscapeString(const std::string& str);
			~DataBase();
		private:
			MYSQL db;
		};

		//存储在数据库中的对象，有唯一标识符
		class DBObject
		{
		public:
			DBObject(DataBase& database, const std::string& table, const std::string& keyname, const std::string& keyvalue)
				:_db(database), tb(table),kn(keyname),kv(_db.EscapeString(keyvalue)), whereStr(std::format(" where {}='{}' limit 1", keyname, kv))
			{}
			template<typename T>
			void Update(const std::string& field, const T& value)
			{
				Update(field, std::to_string(value));
			}
			template<>
			void Update(const std::string& field, const std::string& str)
			{
				_db.Execute(std::format("update {} set {}='{}'", tb, field, _db.EscapeString(str)) + whereStr);
			}
			template<>
			void Update(const std::string& field, const Date& date)
			{
				Update(field, std::format("{:%F}", date));
			}
			template<>
			void Update(const std::string& field, const DateTime& dateTime)
			{
				Update(field, std::format("{:%F %T}", dateTime));
			}
			Value Select(const std::string& field)
			{
				return _db.Execute(std::format("select {} from {}", field, tb) + whereStr)->GetRows()[0][0];
			}
			bool IsValid()
			{
				return !_db.Execute(std::format("select 1 from {}", tb) + whereStr)->IsEmpty();
			}
			void Insert()
			{
				_db.Execute(std::format("insert into {} ({}) values('{}')", tb, kn, kv));
			}
			void Delete()
			{
				_db.Execute(std::format("delete from {}", tb) + whereStr);
			}
		protected:
			DataBase& _db;
			std::string tb, kn, kv;
			std::string whereStr;
		};

		DataBase::DataBase(const std::string& user_name, const std::string& password, const std::string& database, const std::string& characters, const std::string& host, const unsigned int port, const char* unix_socket, const unsigned long client_flag)
		{
			mysql_init(&db);
			if (!mysql_real_connect(&db, host.data(), user_name.data(), password.data(), database.data(), port, unix_socket, client_flag))
				throw MySQLConnectException(mysql_error(&db));
			//设置访问编码
			mysql_set_character_set(&db, characters.data());
		}

		std::optional<QueryResult> DataBase::Execute(const std::string& str)
		{
			if (mysql_real_query(&db, str.data(), str.size()))
				throw MySQLExecuteException(mysql_error(&db));
			MYSQL_RES* result = mysql_store_result(&db);
			if (result)
			{
				QueryResult r(result);
				mysql_free_result(result);
				return r;
			}
			if (mysql_field_count(&db) == 0)//无返回数据，不是查询语句
			{
				return std::nullopt;
			}
			throw MySQLExecuteException(mysql_error(&db));
		}

		std::string DataBase::GetDataBaseName() const noexcept
		{
			return db.db;
		}

		std::string DataBase::EscapeString(const std::string& str)
		{
			char* temp = new char[str.size() * 2 + 1];
			const size_t ret = mysql_real_escape_string(&db, temp, str.data(), str.size());
			if (ret == -1)
				throw MySQLRuntimeException("格式化出现错误！");
			std::string r(temp, ret);
			delete[]temp;
			return r;
		}

		DataBase::~DataBase()
		{
			mysql_close(&db);
		}

		QueryResult::QueryResult(MYSQL_RES* result)
		{
			//处理列
			MYSQL_FIELD* fs = mysql_fetch_fields(result);
			const size_t field_count = mysql_num_fields(result);
			fields.reserve(field_count);
			for (unsigned int i = 0; i < field_count; ++i)
			{
				fields.push_back(fs[i].name);
			}
			//处理行
			MYSQL_ROW row;
			while (row = mysql_fetch_row(result))
			{
				Row r;
				for (size_t i = 0; i < field_count; ++i)
				{
					if (row[i])
						r.data.emplace_back(row[i]);
					else
						r.data.emplace_back("");
				}
				rows.push_back(std::move(r));
			}
		}

		const std::vector<Row>& QueryResult::GetRows() const noexcept
		{
			return rows;
		}

		const std::vector<std::string>& QueryResult::GetFields() const noexcept
		{
			return fields;
		}

		inline bool QueryResult::IsEmpty() const noexcept
		{
			return rows.empty();
		}
	}
}