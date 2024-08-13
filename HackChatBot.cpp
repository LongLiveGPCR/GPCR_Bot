#include<iostream>
#include <string>
#include<format>
#include<unordered_set>
#include<httplib.h>
#include<json.hpp>
#include<MessageProcessor.h> 
#include<MySQLLibrary.h>
#include<RuntimeData.h>

using MyStd::MySQL::Date;
using MyStd::MySQL::DateTime;
using namespace std::chrono_literals;
MyStd::MySQL::DataBase db("root", "123456", "hcbot", "utf8mb4");
std::string At(const std::string& nick, const std::string& text)
{
	return "@" + nick + " " + text + "\n";
}
std::string Code(const std::string& s)
{
	return "\n```\n" + s + "\n```\n";
}
class User :public MyStd::MySQL::DBObject
{
public:
	enum Type :uint8_t { NORMAL, CAPITALISM, SOCIALISM };
	static std::string GetTypeString(Type t)
	{
		switch (t)
		{
		case NORMAL:
			return "默认";
		case CAPITALISM:
			return "资本主义";
		case SOCIALISM:
			return "社会主义";
		}
		return "";
	}
	const std::string mTrip;
	User(const std::string& trip) :DBObject(db, "user", "id", trip), mTrip(trip) {}
	unsigned short GetNormalSalary()
	{
		switch ((Type)(uint8_t)Select("type"))
		{
		case CAPITALISM:
			return (unsigned short)Select("productiontechnology") / 6;
		case SOCIALISM:
			return (unsigned short)Select("productiontechnology") / 2;
		default:
			return (unsigned short)Select("productiontechnology") / 3;
		}
	}
};
class Message
{
public:
	std::string mTrip;
	std::string mText;
	Message(const nlohmann::json& j)
		: mTrip(j["trip"].get_ref<const std::string&>()),
		mText(j["text"].get_ref<const std::string&>()) {}
};
//答题
struct Question
{
	std::string mQuestion, mAnalytic;
	char mAnswer;
	Question(const std::string& question, char answer, const std::string& analytic)
		:mQuestion(question), mAnalytic(analytic), mAnswer(answer) {}
};
//所有多人游戏的基类
class MultiplayerGame
{
protected:
	std::unordered_set<std::string> mPlayers;
	const size_t mPlayerCount;
	const std::function<void(const std::string&)> SendMsg;
	const std::function<void()> DeleteThis;
	virtual void OnStart() {}
public:
	MultiplayerGame(size_t playerCount, std::function<void(const std::string&)> sendMsgCallBack, std::function<void()> deleteThisCallBack)
		: mPlayerCount(playerCount), SendMsg(sendMsgCallBack), DeleteThis(deleteThisCallBack) {}
	bool Started()const
	{
		return mPlayers.size() == mPlayerCount;
	}
	void Join(const std::string& id)
	{
		if (!Started())
		{
			if (mPlayers.insert(id).second)
			{
				SendMsg(At(id, "加入游戏成功！"));
				if (Started())
				{
					OnStart();
				}
			}
			else
				SendMsg(At(id, "你已经在游戏中，请勿重复加入！"));
		}
		else
			SendMsg(At(id, "游戏已经开始，无法加入！"));
	}
	void Leave(const std::string& id)
	{
		if (!Started())
		{
			if (mPlayers.erase(id))
			{
				SendMsg(At(id, "退出游戏成功！"));
				if (mPlayers.empty())
				{
					DeleteThis();
				}
			}
			else
				SendMsg(At(id, "你不在游戏中！"));
		}
		else
			SendMsg(At(id, "游戏已经开始，无法退出！"));
	}
};
//双人五子棋
class Gobang final :public MultiplayerGame
{
private:
	enum Chess :uint8_t { EMPTY, BLACK, WHITE };
	Chess board[15][15];
	Chess nextPlayer;
	uint8_t lastX, lastY;
	uint8_t count;//走棋步数
	std::unordered_set<std::string>::iterator nextPlayerIter;
	bool Win()const
	{
		Chess color = board[lastX][lastY];
		uint8_t x = lastX, y = lastY;
		uint8_t m_max, count;
		//y轴
		while (--y >= 0 && board[x][y] == color);
		y++;
		for (count = 1; (++y < 15) && (board[x][y] == color); count++);
		m_max = count;
		//x轴
		x = lastX, y = lastY;
		while (--x >= 0 && board[x][y] == color);
		x++;
		for (count = 1; ++x < 15 && board[x][y] == color; count++);
		if (m_max < count)
			m_max = count;
		//左下到右上
		x = lastX, y = lastY;
		while (x - 1 >= 0 && y - 1 >= 0 && board[x - 1][y - 1] == color)
			x--, y--;
		for (count = 1; x + 1 < 15 && y + 1 < 15 && board[x + 1][y + 1] == color; count++)
			x++, y++;
		if (m_max < count)
			m_max = count;
		//左上到右下
		x = lastX, y = lastY;
		while (x - 1 >= 0 && y + 1 < 15 && board[x - 1][y + 1] == color)
			x--, y++;
		for (count = 1; x + 1 < 15 && y - 1 >= 0 && board[x + 1][y - 1] == color; count++)
			x++, y--;
		if (m_max < count)
			m_max = count;
		return m_max >= 5;
	}
	std::string GetBoardString()const
	{
		std::string res = "\n";
		for (size_t i = 0; i < 15; ++i)
		{
			for (size_t j = 0; j < 15; ++j)
			{
				switch (board[i][j])
				{
				case BLACK:
					res += "●";
					break;
				case WHITE:
					res += "◯";
					break;
				default:
					res += "十";
					break;
				}
			}
			res += '\n';
		}
		return Code(res);
	}
protected:
	virtual void OnStart() override
	{
		MultiplayerGame::OnStart();
		SendMsg("五子棋游戏开始！");
		nextPlayerIter = mPlayers.begin();
		SendMsg(GetBoardString());
		SendMsg(At(*nextPlayerIter, "请发送要下的坐标："));
	}
public:
	Gobang(std::function<void(const std::string&)> sendMsgCallBack, std::function<void()> deleteThisCallBack)
		:MultiplayerGame(2, sendMsgCallBack, deleteThisCallBack)
	{
		nextPlayer = BLACK;
		lastX = lastY = 0;
		count = 0;
		memset(board, EMPTY, sizeof(board));
	}
	void Put(uint8_t x, uint8_t y, const std::string& player)
	{
		if (!Started() || player != *nextPlayerIter)
		{
			SendMsg(At(player, "你不在该房间或还没轮到你！"));
			return;
		}
		if (x >= 15 || y >= 15)
		{
			SendMsg(At(player, "坐标超出范围！"));
			return;
		}
		if (board[x][y] != EMPTY)
		{
			SendMsg(At(player, "此处已有棋子！"));
			return;
		}
		board[x][y] = nextPlayer;
		lastX = x, lastY = y;
		++count;
		if (nextPlayer == BLACK)
		{
			nextPlayer = WHITE;
		}
		else
		{
			nextPlayer = BLACK;
		}
		SendMsg(GetBoardString());
		if (Win())
		{
			SendMsg(At(*nextPlayerIter, "恭喜，你赢了！"));
			++nextPlayerIter;
			if (nextPlayerIter == mPlayers.end())
				nextPlayerIter = mPlayers.begin();
			SendMsg(At(*nextPlayerIter, "你输了！")); 
			DeleteThis();
			return;
		}
		if (count == 255)
		{
			SendMsg("棋盘已满，平局！");
			DeleteThis();
			return;
		}
		++nextPlayerIter;
		if (nextPlayerIter == mPlayers.end())
			nextPlayerIter = mPlayers.begin();
		SendMsg(At(*nextPlayerIter, "请发送要下的坐标："));
	}
};

#define CheckSenderValid \
do\
{\
	User sender(msg.mTrip);\
	if(!sender.IsValid())\
	{\
		ret = At(sender.mTrip, "你没有注册！");\
		return;\
	}\
}while(false)
#define CheckUserValid(u) \
do\
{\
	if (!(u).IsValid())\
	{\
		ret = At(msg.mTrip, "该用户没有注册！");\
		return;\
	}\
}while(false)
#define CheckSenderPermission(n) \
do\
{\
	User sender(msg.mTrip);\
	if(!sender.IsValid())\
	{\
		ret = At(sender.mTrip, "你没有注册！");\
		return;\
	}\
	if ((uint8_t)sender.Select("permission") < (n))\
	{\
		ret = At(sender.mTrip, std::format("你没有权限！需要的最低权限：{}",(n)));\
		return;\
	}\
}while(false)
int main()
{
	SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);//防止电脑休眠
	srand(time(nullptr));
	std::system("chcp 65001");
	httplib::Server svr; 
	MyStd::MessageProcessor<const Message&> processor;
	std::string ret;
	MyStd::RuntimeDataManager data;
	processor.AddMatchProcessor("菜单", [&](const std::smatch&, const Message& msg) {
		static const std::string message = [&]() {
			const auto& menu = processor.GetMatchProcessorsList();
			std::string res = std::format("菜单（共{}个指令)：", menu.size());
			for (const auto& reg : menu)
			{
				res += '\n';
				res += reg.regexStr;
			}
			return res;
			}();
		ret = Code(message);
		});
	processor.AddMatchProcessor("用户列表", [&](const std::smatch&, const Message& msg) {
		auto res = db.Execute("select id from user")->GetRows();
		std::string message = std::format("共有{}个已注册的用户：\n", res.size());
		for (const auto& i : res)
		{
			message += i[0];
			message += '\n';
		}
		ret = At(msg.mTrip, message);
		});
	processor.AddMatchProcessor("注册", [&](const std::smatch&, const Message& msg) {
		User u(msg.mTrip);
		if (u.IsValid())
		{
			ret = At(msg.mTrip, "你已经注册过了！");
			return;
		}
		u.Insert();
		ret = At(msg.mTrip, "注册成功！");
		});
	processor.AddMatchProcessor("注销", [&](const std::smatch&, const Message& msg) {
			CheckSenderValid;
			User(msg.mTrip).Delete();
			ret = At(msg.mTrip, "注销成功！");
		});
	processor.AddMatchProcessor("签到", [&](const std::smatch&, const Message& msg) {
		CheckSenderValid;
		User u(msg.mTrip);

		if ((Date)u.Select("lastsignindate") != std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()))
		{
			int money = rand() % 6 + 5;
			u.Update("lastsignindate", std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
			u.Update("money", (long long)u.Select("money") + money);
			ret = At(msg.mTrip, std::format("签到成功！奖励{}金币！", money));
			if ((uint8_t)u.Select("type") == User::SOCIALISM)
			{
				ret += At(msg.mTrip, "技能【红色宣传】发动！奖励20幸福度！");
				u.Update("happiness", std::min((int)u.Select("happiness") + 20, 255));
			}
		}
		else
			ret = At(msg.mTrip, "你今天已经签过到了！");
		});
	processor.AddMatchProcessor(R"(设置权限\s*([^']{6})\s*(\d+))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderPermission(255);
		User u(m.str(1));
		CheckUserValid(u);
		u.Update("permission", m.str(2));
		ret = At(msg.mTrip, "已经把该用户的权限设为" + m.str(2) + "！");
		});
	processor.AddMatchProcessor(R"(设置金币\s*([^']{6})\s*(\-?\d+))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderPermission(253);
		User u(m.str(1));
		CheckUserValid(u);
		u.Update("money", m.str(2));
		ret = At(msg.mTrip, "已经把该用户的金币设为" + m.str(2) + "！");
		});
	processor.AddMatchProcessor(R"(转账\s*([^']{6})\s*(\d+))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderValid;
		User u(msg.mTrip);
		const long long money = stoll(m.str(2));
		if ((long long)u.Select("money") >= money)
		{
			User u2(m.str(1));
			CheckUserValid(u2);
			u.Update("money", (long long)u.Select("money") - money);
			u2.Update("money", (long long)u2.Select("money") + money);
			ret = At(msg.mTrip, "转账成功！\n\n") + At(u2.mTrip, std::format("{}给你转了{}金币！快去感谢他吧！", u.mTrip, money));
		}
		else
			ret = At(msg.mTrip, "您的金币不足！");
		});
	processor.AddMatchProcessor(R"(梭哈\s*(\d+))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderValid;
		User u(msg.mTrip);
		long long money = stoll(m.str(1));
		if ((long long)u.Select("money") >= money)
		{
			if (rand() % 2)
			{
				u.Update("money", (long long)u.Select("money") + money);
				ret = At(msg.mTrip, "梭哈成功！你获得了" + m.str(1) + "金币！");
			}
			else
			{
				u.Update("money", (long long)u.Select("money") - money);
				ret = At(msg.mTrip, "梭哈失败！你损失了" + m.str(1) + "金币！");
			}
		}
		else
			ret = At(msg.mTrip, "您的金币不足！");
		});
	processor.AddMatchProcessor("查询", [&](const std::smatch&, const Message& msg) {
		CheckSenderValid;
		User u(msg.mTrip);
		ret = At(msg.mTrip,
			std::format("您的账户信息如下：\n识别码：{}\n金币：{}\n权限：{}\n工人：{}\n军队：{}\n"
				"生产科技：{}\n军事科技：{}\n工人工资：{}\n幸福度：{}\n政府类型：{}",
				u.mTrip, (long long)u.Select("money"), (uint8_t)u.Select("permission"), (unsigned long)u.Select("worker"), (unsigned long)u.Select("army"), (unsigned short)u.Select("productiontechnology"),
				(unsigned short)u.Select("militarytechnology"), (unsigned short)u.Select("salary"), (uint8_t)u.Select("happiness"), User::GetTypeString((User::Type)(uint8_t)u.Select("type"))));
		});
	processor.AddMatchProcessor(R"(查询\s*([^']{6}))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderPermission(253);
		User u(m.str(1));
		CheckUserValid(u);
		ret = At(msg.mTrip,
			std::format("该用户的信息如下：\n识别码：{}\n金币：{}\n权限：{}\n工人：{}\n军队：{}\n"
				"生产科技：{}\n军事科技：{}\n工人工资：{}\n幸福度：{}\n政府类型：{}",
				u.mTrip, (long long)u.Select("money"), (uint8_t)u.Select("permission"), (unsigned long)u.Select("worker"), (unsigned long)u.Select("army"), (unsigned short)u.Select("productiontechnology"),
				(unsigned short)u.Select("militarytechnology"), (unsigned short)u.Select("salary"), (uint8_t)u.Select("happiness"), User::GetTypeString((User::Type)(uint8_t)u.Select("type"))));
		});
	processor.AddMatchProcessor(R"(执行SQL语句\s*(.+))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderPermission(255);
		try
		{
			static MyStd::MySQL::DataBase userdb("root", "123456", "hcbot", "utf8mb4");//用于用户操作
			if (auto res = userdb.Execute(m.str(1)); res.has_value())
			{
				std::string message;
				for (const std::string& str : res->GetFields())
				{
					message += std::format("{:20}", str);
				}
				message += '\n';
				for (const auto& row : res->GetRows())
				{
					for (const std::string& str : row.GetData())
					{
						message += std::format("{:20}", str);
					}
					message += '\n';
				}
				ret = At(msg.mTrip, Code(message));
			}
			else
			{
				ret = At(msg.mTrip, "执行成功！");
			}
		}
		catch (const MyStd::MySQL::MySQLExecuteException& ex)
		{
			ret = At(msg.mTrip, std::string("执行失败，错误信息：") + ex.what());
		}
		});
	processor.AddMatchProcessor(R"(招募工人\s*(\d+))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderValid;
		User u(msg.mTrip);
		long long money = (long long)u.Select("money");
		unsigned int worker = stoul(m.str(1));
		if ((Date)u.Select("lastrecruitworkerdate") != std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()))
		{
			u.Update("lastrecruitworkerdate", std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
			u.Update("recruitableworker", (User::Type)(uint8_t)u.Select("type") == User::CAPITALISM ? 300 : 200);
		}
		unsigned short recruitableWorker = u.Select("recruitableworker");
		if (worker > recruitableWorker)
		{
			ret = At(msg.mTrip, std::format("招募失败！您今日剩余最多可招募{}工人！", recruitableWorker));
			return;
		}
		long long cost;
		if ((uint8_t)u.Select("type") == User::CAPITALISM)
		{
			cost = worker * 3 / 2 + worker % 2;
			//ret = At(msg.mTrip, "技能【资本积累】发动！招募成本降低为1.5！");
		}
		else
		{
			cost = worker * 2;
		}
		if (money >= cost)
		{
			u.Update("money", money - cost);
			u.Update("worker", (unsigned long)u.Select("worker") + worker);
			u.Update("recruitableworker", recruitableWorker - worker);
			ret = At(msg.mTrip, std::format("招募工人成功！花费{}金币！", cost));
		}
		else
			ret = At(msg.mTrip, "金币不足！");
		});
	processor.AddMatchProcessor("研究生产科技", [&](const std::smatch& m, const Message& msg) {
		CheckSenderValid;
		User u(msg.mTrip);
		long long money = (long long)u.Select("money");
		unsigned int level = (unsigned short)u.Select("productiontechnology");
		if (level >= 500)
		{
			ret = At(msg.mTrip, "您的生产科技已经满级！");
			return;
		}
		long long cost = std::pow(2ll, (level - 100) / 10 + 3);
		if ((uint8_t)u.Select("type") == User::CAPITALISM)
		{
			cost = cost * 6 / 10;
			//ret = At(msg.mTrip, "技能【科技发达】发动！研究成本降低40%！");
		}
		if (money >= cost)
		{
			u.Update("money", money - cost);
			u.Update("ProductionTechnology", level + 1);
			ret = At(msg.mTrip, std::format("研究成功！花费{}金币！", cost));
		}
		else
			ret = At(msg.mTrip, "金币不足！");
		});
	processor.AddMatchProcessor("生产", [&](const std::smatch& m, const Message& msg) {
		CheckSenderValid;
		User u(msg.mTrip);
		const auto cd = ((User::Type)(uint8_t)u.Select("type") == User::CAPITALISM ? 10min : 15min);
		if (std::chrono::system_clock::now() - (DateTime)u.Select("lastproducetime") <= cd)
		{
			ret = At(msg.mTrip, "生产正在冷却中！");
			return;
		}
		unsigned long long worker = (unsigned long)u.Select("worker");
		if (worker > 0)
		{
			unsigned short technology = (unsigned short)u.Select("productiontechnology");
			unsigned short salary = (unsigned short)u.Select("salary"), normalSalary = u.GetNormalSalary();
			long long res = worker * (technology - salary) / 100;
			int happinessres = (salary - normalSalary) * 10 / normalSalary;
			if (happinessres > 5)
				happinessres = 5;
			ret = At(msg.mTrip, std::format(
				"\n生产\n工人数量：{}\n生产科技：{}\n工人工资：{}\n实际收入：{}*({}-{})/100={}\n"
				"标准工资：{}\n幸福度增加：{}\n{}分钟后可进行下一次生产！",
				worker, technology, salary, worker, technology, salary, res, normalSalary,
				happinessres, cd.count()));
			u.Update("money", (long long)u.Select("money") + res);
			u.Update("happiness", std::max(std::min((int)u.Select("happiness") + happinessres, 255), 0));
			u.Update("lastproducetime", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
		}
		else
			ret = At(msg.mTrip, "你没有工人！");
		});
	processor.AddMatchProcessor(R"(设置工资\s*(\d+))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderValid;
		User u(msg.mTrip);
		unsigned short newSalary = stoul(m.str(1)), technology = (unsigned short)u.Select("productiontechnology"),
			normalSalary = u.GetNormalSalary();
		if (newSalary <= technology)
		{
			u.Update("salary", newSalary);
			int happinessres = (newSalary - normalSalary) * 10 / normalSalary;
			if (happinessres > 5)
				happinessres = 5;
			ret = At(msg.mTrip, std::format("设置成功！\n当前工资：{}\n当前标准工资：{}\n"
				"每次生产幸福度增加：{}", newSalary, normalSalary, happinessres));
		}
		else
			ret = At(msg.mTrip, "工资不能高于生产科技！");
		});
	processor.AddMatchProcessor(R"(招募军队\s*(\d+))",[&](const std::smatch& m, const Message& msg){
		CheckSenderValid;
		User u(msg.mTrip);
		long long money = (long long)u.Select("money");
		long long army = std::stoull(m.str(1));
		long long cost;
		if ((uint8_t)u.Select("type") == User::SOCIALISM)
		{
			cost = army * 3 / 2 + army % 2;
			//ret = At(msg.mTrip, "技能【踊跃参军】发动！招募成本降低为1.5！");
		}
		else
		{
			cost = army * 2;
		}
		if (money >= cost)
		{
			u.Update("money", money - cost);
			u.Update("army", (unsigned long)u.Select("army") + army);
			ret = At(msg.mTrip, std::format("招募军队成功！花费{}金币！", cost));
		}
		else
			ret = At(msg.mTrip, "金币不足！");
		});
	processor.AddMatchProcessor("研究军事科技", [&](const std::smatch& m, const Message& msg) {
		CheckSenderValid;
		User u(msg.mTrip);
		long long money = (long long)u.Select("money");
		unsigned int level = (unsigned short)u.Select("militarytechnology");
		if (level >= 500)
		{
			ret = At(msg.mTrip, "您的军事科技已经满级！");
			return;
		}
		long long cost = std::pow(2ll, (level - 100) / 10 + 3);
		if ((uint8_t)u.Select("type") == User::CAPITALISM)
		{
			cost = cost * 6 / 10;
			//ret = At(msg.mTrip, "技能【科技发达】发动！研究成本降低40%！");
		}
		if (money >= cost)
		{
			u.Update("money", money - cost);
			u.Update("militarytechnology", level + 1);
			ret = At(msg.mTrip, std::format("研究成功！花费{}金币！", cost));
		}
		else
			ret = At(msg.mTrip, "金币不足！");
		});
	processor.AddMatchProcessor(R"(攻击\s*([^']{6})\s*(\d+))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderValid;
		User attacker(msg.mTrip);
		User defender(m.str(1));
		if (attacker.mTrip == defender.mTrip)
		{
			ret = At(msg.mTrip, "不能攻击自己！");
			return;
		}
		CheckUserValid(defender);
		if (std::chrono::system_clock::now() - (DateTime)defender.Select("lastlosttime") <= 6h)
		{
			ret = At(msg.mTrip, "对方在失败保护期中，不能进攻！");
			return;
		}
		if (std::chrono::system_clock::now() - (DateTime)attacker.Select("lastattacktime") <= 90min)
		{
			ret = At(msg.mTrip, "攻击还在冷却中！");
			return;
		}
		unsigned long army1 = attacker.Select("army");
		unsigned long attackArmy = stoull(m.str(2));
		if (attackArmy == 0)
		{
			ret = At(msg.mTrip, "军队数不能为0！");
			return;
		}
		if (army1 < attackArmy)
		{
			ret = At(msg.mTrip, "军队不足！");
			return;
		}

		//正式开始攻击
		attacker.Update("lastattacktime", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
		At(defender.mTrip, attacker.mTrip + "攻击了你！");
		User::Type type1 = (User::Type)(uint8_t)attacker.Select("type");
		User::Type type2 = (User::Type)(uint8_t)defender.Select("type");
		//进攻方信息
		unsigned short technology1 = attacker.Select("militarytechnology");
		int happiness1 = attacker.Select("happiness");
		uint8_t luck1 = rand() % 25 + 100;
		long double skill1 = 1;
		if (type1 == User::SOCIALISM && type2 != User::SOCIALISM)
		{
			ret += At(attacker.mTrip, "技能【解放世界】发动！获得50%战斗力加成！\n");
			skill1 *= 1.5;
		}
		long double point1 = skill1 * attackArmy * technology1 * happiness1 * luck1;
		//防守方信息
		unsigned long defendArmy = defender.Select("army");
		unsigned short technology2 = defender.Select("militarytechnology");
		int happiness2 = defender.Select("happiness");
		uint8_t luck2 = rand() % 25 + 100;
		long double skill2 = 1;
		if (type2 == User::SOCIALISM)
		{
			ret += At(defender.mTrip, "技能【保家卫国】发动！获得80%战斗力加成！\n");
			skill2 *= 1.8;
		}
		long double point2 = skill2 * defendArmy * technology2 * happiness2 * luck2 * 1.1;

		std::string winner;
		std::string res1 = "攻击者\n", res2 = "防御者\n";
		unsigned long damage1, damage2;
		long long pmoney = 0;
		unsigned long pworker = 0, parmy = 0;
		if (point1 > point2)
		{
			winner = "攻击者";
			damage1 = attackArmy * static_cast<long double>(point2) / point1;
			damage2 = defendArmy;
			pmoney = (long long)defender.Select("money") * 3 / 10;
			pworker = (unsigned long)defender.Select("worker") * 3 / 10;
			parmy = defendArmy / 10;
			defender.Update("lastlosttime", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
			attacker.Update("army", army1 - damage1 + parmy);
			res1 += std::format("军队：-{}+{}\n", damage1, parmy);
			defender.Update("army", (unsigned long)defender.Select("army") - damage2);
			res2 += std::format("军队：-{}\n", damage2);
			if (type1 == User::SOCIALISM)
			{
				if (type2 != User::SOCIALISM)
				{
					ret += At(attacker.mTrip, "技能【万民归附】发动！获得工人数翻倍！\n");
					pworker *= 2;
					ret += At(attacker.mTrip, "技能【解放世界】发动！幸福度+2！\n");
					attacker.Update("happiness", std::min(happiness1 + 2, 255));
					res1 += "幸福度：+2\n";
				}
				if (type2 == User::CAPITALISM)
				{
					ret += At(attacker.mTrip, "技能【赤化运动】发动！防御者政府类型变为默认！\n");
					defender.Update("type", User::NORMAL);
				}
				else if (type2 == User::NORMAL && (unsigned short)defender.Select("productiontechnology") >= 130)
				{
					ret += At(attacker.mTrip, "技能【赤化运动】发动！防御者政府类型变为社会主义！\n");
					defender.Update("type", User::SOCIALISM);
				}
			}
			else
			{
				attacker.Update("happiness", std::max(happiness1 - 2, 0));
				res1 += "幸福度：-2\n";
			}
			if (type1 == User::CAPITALISM)
			{
				ret += At(attacker.mTrip, "技能【资本输出】发动！获得金币是默认的2倍，工人是默认的1.5倍！\n");
				pmoney *= 2;
				pworker = pworker * 3 / 2;
			}
			attacker.Update("money", (long long)attacker.Select("money") + pmoney);
			res1 += std::format("金币：+{}", pmoney);
			defender.Update("money", (long long)defender.Select("money") - pmoney);
			res2 += std::format("金币：-{}\n", pmoney);
			defender.Update("worker", (unsigned long)defender.Select("worker") - pworker);
			res2 += std::format("工人：-{}", pworker);
			if (type2 == User::SOCIALISM)
			{
				ret += At(defender.mTrip, "技能【宁死不屈】发动！攻击者工人不会增加！\n");
			}
			else
			{
				attacker.Update("worker", (unsigned long)attacker.Select("worker") + pworker);
				res1 += std::format("\n工人：+{}", pworker);
			}
		}
		else
		{
			winner = "防御者";
			damage1 = attackArmy;
			damage2 = defendArmy * static_cast<long double>(point1) / point2;
			parmy = attackArmy / 5;
			attacker.Update("army", army1 - damage1);
			res1 += std::format("军队：-{}\n", damage1);
			defender.Update("army", defendArmy - damage2 + parmy);
			res2 += std::format("军队：-{}+{}\n", damage2, parmy);
			if (type1 == User::SOCIALISM)
			{
				ret += At(attacker.mTrip, "技能【解放世界】发动！幸福度不会下降！\n");
			}
			else
			{
				attacker.Update("happiness", std::max(happiness1 - 5, 0));
				res1 += "幸福度：-5";
			}
			defender.Update("happiness", std::min(happiness2 + 2, 255));
			res2 += "幸福度：+2";
		}
		ret += Code(std::format("战报\n攻击者{} VS 防御者{}\n{}获胜！\n\n"
			"攻击者\n军队：{}\n军事科技：{}\n幸福度：{}\n运气：{}\n技能加成：{}\n总战力：{}*{}*{}*{}*{}={:.0f}\n战损：{}\n\n"
			"防御者\n军队：{}\n军事科技：{}\n幸福度：{}\n运气：{}\n技能加成：{}\n防御加成：1.1\n总战力：{}*{}*{}*{}*{}*1.1={:.0f}\n战损：{}",
			attacker.mTrip, defender.mTrip, winner,
			attackArmy, technology1, happiness1, luck1, skill1, attackArmy, technology1, happiness1, luck1, skill1, point1, damage1,
			defendArmy, technology2, happiness2, luck2, skill2, defendArmy, technology2, happiness2, luck2, skill2, point2, damage2)
			+ "\n\n" + res1 + "\n\n" + res2);
		});
	processor.AddMatchProcessor(R"(改变政府类型\s*(.+))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderValid;
		User u(msg.mTrip);
		if ((Date)u.Select("lastchangedate") != std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()))
		{
			if (User::GetTypeString((User::Type)(uint8_t)u.Select("type")) == m.str(1))
			{
				ret = At(msg.mTrip, "修改失败！您现在已经是该政府类型！");
				return;
			}
			if (m.str(1) == "默认")
			{
				ret = At(msg.mTrip, "修改成功！您现在的政府类型：默认\n技能：无");
				u.Update("lastchangedate", std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
				u.Update("type", User::NORMAL);
			}
			else if (m.str(1) == "资本主义")
			{
				if ((unsigned short)u.Select("productiontechnology") >= 150)
				{
					ret = At(msg.mTrip, "修改成功！您现在的政府类型：资本主义\n技能：\n"
						"【资本积累】工人招募成本下降为1.5，每日可招募工人上限上升到300\n"
						"【996】生产冷却缩短为10min，工人标准工资下降为生产科技的1/6\n"
						"【资本输出】攻击成功时，掠夺的金币是默认的2倍，工人是默认的1.5倍\n"
						"【科技发达】研究生产科技和军事科技的成本下降40%");
					u.Update("lastchangedate", std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
					u.Update("type", User::CAPITALISM);
				}
				else
					ret = At(msg.mTrip, "生产科技不满足要求！");
			}
			else if (m.str(1) == "社会主义")
			{
				if ((unsigned short)u.Select("productiontechnology") >= 200)
				{
					ret = At(msg.mTrip, "修改成功！您现在的政府类型：社会主义\n技能：\n"
						"【红色宣传】每日签到时赠送20幸福度\n"
						"【禁止剥削】工人标准工资上升为生产科技的50%\n"
						"【踊跃参军】军队招募成本下降为1.5\n"
						"【解放世界】攻击非社会主义国家时，战斗力获50%加成，若攻击成功幸福度上升2，攻击失败则不变\n"
						"【万民归附】攻击非社会主义国家成功时，获得的工人是正常的2倍\n"
						"【赤化运动】攻击资本主义国家成功时，对方自动转变为默认国家；攻击默认国家成功时，若对方生产科技满足要求，则自动转变为社会主义国家\n"
						"【保家卫国】防御时战斗力获得80%加成\n"
						"【宁死不屈】防御失败时，工人只会减少，不会被对方掠夺");
					u.Update("lastchangedate", std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
					u.Update("type", User::SOCIALISM);
				}
				else
					ret = At(msg.mTrip, "生产科技不满足要求！");
			}
			else
				ret = At(msg.mTrip, "没有这种政府类型！");
		}
		else
			ret = At(msg.mTrip, "一天只能修改一次政府类型！");
		});
	processor.AddMatchProcessor("随机句子", [&](const std::smatch& m, const Message& msg) {
		httplib::SSLClient cli("api.vvhan.com");
		if (auto res = cli.Get("/api/ian/rand"))
		{
			if (res->status == 200)
				ret = At(msg.mTrip, res->body);
			else
				ret = At(msg.mTrip, std::format("获取随机句子失败！错误码：{}", res->status));
		}
		else
		{
			ret = At(msg.mTrip, "获取随机句子失败！HTTP错误: " + httplib::to_string(res.error()));
		}
		});
	processor.AddMatchProcessor("答题", [&](const std::smatch& m, const Message& msg) {
		Question* q = data.GetData<Question>(msg.mTrip + "_question");
		if (q == nullptr)
		{
			httplib::SSLClient cli("apis.tianapi.com");
			auto res = cli.Get("/baiketiku/index?key=65a0dd35b088ad62e411385f298bfb9d");
			if (!res)
			{
				ret = At(msg.mTrip, "获取题目失败！HTTP错误: " + httplib::to_string(res.error()));
				return;
			}
			if (res->status != 200)
			{
				ret = At(msg.mTrip, std::format("获取题目失败！错误码：{}", res->status));
				return;
			}
			nlohmann::json j = nlohmann::json::parse(res->body);
			if (j["code"] != 200)
			{
				ret = At(msg.mTrip, "获取题目失败！错误信息：" + j["msg"].get_ref<const std::string&>());
				return;
			}
			const nlohmann::json& ques = j["result"];
			q = data.AddData<Question>(msg.mTrip + "_question",
				std::format("{}\nA.{}\nB.{}\nC.{}\nD.{}\n回复“选X”作答",
					ques["title"].get_ref<const std::string&>(),
					ques["answerA"].get_ref<const std::string&>(),
					ques["answerB"].get_ref<const std::string&>(),
					ques["answerC"].get_ref<const std::string&>(),
					ques["answerD"].get_ref<const std::string&>()),
				ques["answer"].get_ref<const std::string&>().front(),
				ques["analytic"].get_ref<const std::string&>()
			);
		}
		ret = At(msg.mTrip, q->mQuestion);
		});
	processor.AddMatchProcessor(R"(选[a-dA-D])", [&](const std::smatch& m, const Message& msg) {
		if (auto q = data.GetData<Question>(msg.mTrip + "_question"))
		{
			if (toupper(m.str(0).back()) == q->mAnswer)
			{
				ret = At(msg.mTrip, "恭喜，回答正确！");
			}
			else
			{
				ret = At(msg.mTrip, "很遗憾，回答错误！");
			}
			ret += At(msg.mTrip, std::format("正确答案：{}\n解析：{}", q->mAnswer, q->mAnalytic));
			data.RemoveData(q);
		}
		else
			ret = At(msg.mTrip, "你还没有开始答题！");
		});
	processor.AddMatchProcessor("历史上的今天", [&](const std::smatch& m, const Message& msg) {
		auto res = db.Execute("select * from httpcache where name='history' limit 1")->GetRows().front();
		if (std::chrono::system_clock::now() == (Date)res[2])
		{
			ret = At(msg.mTrip, res[1]);
		}
		else
		{
			httplib::SSLClient cli("query.asilu.com");
			if (auto res = cli.Get("/today/list/"))
			{
				if (res->status == 200)
				{
					nlohmann::json j = nlohmann::json::parse(res->body);
					std::string message = "历史上的今天\n"
						+ j["month"].get_ref<const std::string&>() + "月"
						+ j["day"].get_ref<const std::string&>() + "日";
					for (const nlohmann::json& js : j["data"])
					{
						message += "\n\n";
						message += js["title"];
						message += '\n';
						message += js["link"];
					}
					ret = At(msg.mTrip, message);
					db.Execute(std::format("update httpcache set data='{}',updatedate='{:%F}' where name='history' limit 1",
						db.EscapeString(message).data(), std::chrono::system_clock::now()));
				}
				else
					ret = At(msg.mTrip, std::format("获取历史上的今天失败！错误码：{}", res->status));
			}
			else
			{
				ret = At(msg.mTrip, "获取历史上的今天失败！HTTP错误: " + httplib::to_string(res.error()));
			}
		}
		});
	processor.AddMatchProcessor(R"(天气\s*(.+))", [&](const std::smatch& m, const Message& msg) {
		httplib::Client cl("v0.yiketianqi.com");
		if (auto res = cl.Get("/api?unescape=1&version=v92&appid=43656176&appsecret=I42og6Lm&city=" + m.str(1)))
		{
			if (res->status == 200)
			{
				nlohmann::json j = nlohmann::json::parse(res->body);
				if (auto iter = j.find("errmsg"); iter != j.end())
				{
					ret = At(msg.mTrip, "获取天气失败！错误信息：" + iter->get_ref<const std::string&>());
				}
				else
				{
					const nlohmann::json& now = j["data"][0];
					ret = At(msg.mTrip, std::format(
						"城市：{}({})\n更新时间：{}\n天气：{}\n"
						"当前温度：{}℃\n今日温度：{}℃-{}℃\n湿度：{}\n"
						"能见度：{}\n气压：{}hPa\n""风向：{}/{}\n风速：{}\n"
						"风力：{}\n空气质量指数：{}\n空气质量等级：{}\n"
						"日出日落：{}/{}",
						j["city"].get_ref<const std::string&>(),
						j["cityEn"].get_ref<const std::string&>(),
						j["update_time"].get_ref<const std::string&>(),
						now["wea"].get_ref<const std::string&>(),
						now["tem"].get_ref<const std::string&>(),
						now["tem1"].get_ref<const std::string&>(),
						now["tem2"].get_ref<const std::string&>(),
						now["humidity"].get_ref<const std::string&>(),
						now["visibility"].get_ref<const std::string&>(),
						now["pressure"].get_ref<const std::string&>(),
						now["win"][0].get_ref<const std::string&>(),
						now["win"][1].get_ref<const std::string&>(),
						now["win_meter"].get_ref<const std::string&>(),
						now["win_speed"].get_ref<const std::string&>(),
						now["air"].get_ref<const std::string&>(),
						now["air_level"].get_ref<const std::string&>(),
						now["sunrise"].get_ref<const std::string&>(),
						now["sunset"].get_ref<const std::string&>()
					));
				}
			}
			else
				ret = At(msg.mTrip, std::format("获取天气失败！错误码：{}", res->status));
		}
		else
		{
			ret = At(msg.mTrip, "获取天气失败！HTTP错误: " + httplib::to_string(res.error()));
		}
		});
	processor.AddMatchProcessor(R"(加入五子棋房间\s*(.+?)\s*)", [&](const std::smatch& m, const Message& msg) {
		if (Gobang* g = data.GetData<Gobang>(m.str(1) + "_game"))
		{
			g->Join(msg.mTrip);
		}
		else
			ret = At(msg.mTrip, "该房间不存在！");
		});
	processor.AddMatchProcessor(R"(离开五子棋房间\s*(.+?)\s*)", [&](const std::smatch& m, const Message& msg) {
		if (Gobang* g = data.GetData<Gobang>(m.str(1) + "_game"))
		{
			g->Leave(msg.mTrip);
		}
		else
			ret = At(msg.mTrip, "该房间不存在！");
		});
	processor.AddMatchProcessor(R"(创建五子棋房间\s*(.+?)\s*)", [&](const std::smatch& m, const Message& msg) {
		const std::string id = m.str(1) + "_game";
		if (data.GetData<MultiplayerGame>(id) == nullptr)
		{
			data.AddData<Gobang>(m.str(1) + "_game",
				[&](const std::string& msg) {
					ret += msg + '\n';
				},
				[&, id]() {
					data.RemoveData(id);
				})->Join(msg.mTrip);
			ret += At(msg.mTrip, "创建成功，你已经自动加入该房间！");
		}
		else
			ret = At(msg.mTrip, "该房间已存在！");
		});
	processor.AddMatchProcessor(R"(下\s*(.+?)\s*(\d+)\s*(\d+))", [&](const std::smatch& m, const Message& msg) {
		if (Gobang* g = data.GetData<Gobang>(m.str(1) + "_game"))
		{
			g->Put(stoul(m.str(2)), stoul(m.str(3)), msg.mTrip);
		}
		else
			ret = At(msg.mTrip, "该房间不存在！");
		});
	processor.AddMatchProcessor("运行时数据列表", [&](const std::smatch& m, const Message& msg) {
		CheckSenderPermission(255);
		ret = std::format("运行时数据（共{}个）：", data.data.size());
		for (const auto& [key, value] : data.data)
		{
			ret += "\n" + key;
		}
		ret = Code(ret);
		});
	processor.AddMatchProcessor(R"(删除运行时数据\s*(.+))", [&](const std::smatch& m, const Message& msg) {
		CheckSenderPermission(255);
		if (data.RemoveData(m.str(1)))
			ret = At(msg.mTrip, "已删除！");
		else
			ret = At(msg.mTrip, "该数据不存在！");
		});

	svr.Post("/", [&](const httplib::Request& req, httplib::Response& res) {
		std::cout << "收到请求，内容：" << req.body << std::endl;
		res.set_header("Access-Control-Allow-Origin", "*");
		ret.clear();
		nlohmann::json j = nlohmann::json::parse(req.body);
		if (j["cmd"].get_ref<const std::string&>() == "chat" && j.count("trip"))
		{
			Message msg(j);
			processor.ProcessMessage(msg.mText, msg);
		}
		if (j["cmd"].get_ref<const std::string&>() == "info"
			&& j["type"].get_ref<const std::string&>() == "whisper" && j.count("trip"))
		{
			Message msg(j);
			msg.mText = msg.mText.substr(msg.mText.find_first_of(':') + 2);
			processor.ProcessMessage(msg.mText, msg);
		}
		res.set_content(ret, "text/plain");
		});
	std::cout << "Server is running on http://localhost:8080" << std::endl;
	svr.listen("localhost", 8080);
	SetThreadExecutionState(ES_CONTINUOUS);//取消防止电脑休眠
	return 0;
}
