/*		 +------------------------------------+
 *		 | Inspire Internet Relay Chat Daemon |
 *		 +------------------------------------+
 *
 *	InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *			  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <sqlite3.h>
#include "sql.h"

/* $ModDesc: sqlite3 provider */
/* $CompileFlags: pkgconfversion("sqlite3","3.3") pkgconfincludes("sqlite3","/sqlite3.h","") */
/* $LinkerFlags: pkgconflibs("sqlite3","/libsqlite3.so","-lsqlite3") */
/* $NoPedantic */

class SQLConn;
typedef std::map<std::string, reference<SQLConn> > ConnMap;

class SQLite3Result : public SQLResult
{
 public:
	int currentrow;
	int rows;
	std::vector<std::vector<std::string> > fieldlists;

	SQLite3Result() : currentrow(0), rows(0)
	{
	}

	~SQLite3Result()
	{
	}

	virtual int Rows()
	{
		return rows;
	}

	virtual bool GetRow(std::vector<std::string>& result)
	{
		if (currentrow < rows)
		{
			result.assign(fieldlists[currentrow].begin(), fieldlists[currentrow].end());
			currentrow++;
			return true;
		}
		else
		{
			result.clear();
			return false;
		}
	}
};

class SQLConn : public refcountbase
{
 private:
	sqlite3* conn;
	reference<ConfigTag> config;

 public:
	SQLConn(ConfigTag* tag) : config(tag)
	{
		std::string host = tag->getString("hostname");
		if (sqlite3_open_v2(host.c_str(), &conn, SQLITE_OPEN_READWRITE, 0) != SQLITE_OK)
		{
			ServerInstance->Logs->Log("m_sqlite3",DEFAULT, "WARNING: Could not open DB with id: " + tag->getString("id"));
			conn = NULL;
		}
	}

	~SQLConn()
	{
		sqlite3_interrupt(conn);
		sqlite3_close(conn);
	}

	void Query(SQLQuery* query)
	{
		SQLite3Result res;
		sqlite3_stmt *stmt;
		int err = sqlite3_prepare_v2(conn, query->query.c_str(), query->query.length(), &stmt, NULL);
		if (err != SQLITE_OK)
		{
			SQLerror error(SQL_QSEND_FAIL, sqlite3_errmsg(conn));
			query->OnError(error);
			return;
		}
		int cols = sqlite3_column_count(stmt);
		while (1)
		{
			err = sqlite3_step(stmt);
			if (err == SQLITE_ROW)
			{
				// Add the row
				res.fieldlists.resize(res.rows + 1);
				res.fieldlists[res.rows].resize(cols);
				for(int i=0; i < cols; i++)
				{
					const char* txt = (const char*)sqlite3_column_text(stmt, i);
					res.fieldlists[res.rows][i] = txt ? txt : "";
				}
				res.rows++;
			}
			else if (err == SQLITE_DONE)
			{
				query->OnResult(res);
				break;
			}
			else
			{
				SQLerror error(SQL_QREPLY_FAIL, sqlite3_errmsg(conn));
				query->OnError(error);
				break;
			}
		}
		sqlite3_finalize(stmt);
	}
};

class SQLiteProvider : public SQLProvider
{
 public:
	ConnMap hosts;

	SQLiteProvider(Module* Parent) : SQLProvider(Parent, "SQL/SQLite") {}

	std::string FormatQuery(std::string q, ParamL p)
	{
		std::string res;
		unsigned int param = 0;
		for(std::string::size_type i = 0; i < q.length(); i++)
		{
			if (q[i] != '?')
				res.push_back(q[i]);
			else
			{
				// TODO numbered parameter support ('?1')
				if (param < p.size())
				{
					char* escaped = sqlite3_mprintf("%q", p[param++].c_str());
					res.append(escaped);
					sqlite3_free(escaped);
				}
			}
		}
		return res;
	}

	std::string FormatQuery(std::string q, ParamM p)
	{
		std::string res;
		for(std::string::size_type i = 0; i < q.length(); i++)
		{
			if (q[i] != '$')
				res.push_back(q[i]);
			else
			{
				std::string field;
				i++;
				while (i < q.length() && isalpha(q[i]))
					field.push_back(q[i++]);
				i--;

				char* escaped = sqlite3_mprintf("%q", p[field].c_str());
				res.append(escaped);
				sqlite3_free(escaped);
			}
		}
		return res;
	}
	
	void submit(SQLQuery* query)
	{
		ConnMap::iterator iter = hosts.find(query->dbid);
		if (iter == hosts.end())
		{
			SQLerror err(SQL_BAD_DBID);
			query->OnError(err);
		}
		else
		{
			iter->second->Query(query);
		}
		delete query;
	}
};

class ModuleSQLite3 : public Module
{
 private:
	SQLiteProvider sqlserv;

 public:
	ModuleSQLite3()
	: sqlserv(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(sqlserv);

		ReadConf();

		Implementation eventlist[] = { I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModuleSQLite3()
	{
	}

	void ReadConf()
	{
		sqlserv.hosts.clear();
		ConfigTagList tags = ServerInstance->Config->ConfTags("database");
		for(ConfigIter i = tags.first; i != tags.second; i++)
		{
			sqlserv.hosts.insert(std::make_pair(i->second->getString("id"), new SQLConn(i->second)));
		}
	}

	void OnRehash(User* user)
	{
		ReadConf();
	}

	Version GetVersion()
	{
		return Version("sqlite3 provider", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSQLite3)
