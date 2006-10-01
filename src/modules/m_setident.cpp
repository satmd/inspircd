#include "users.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides support for the SETIDENT command */

class cmd_setident : public command_t
{
 public:
 cmd_setident (InspIRCd* Instance) : command_t(Instance,"SETIDENT", 'o', 1)
	{
		this->source = "m_setident.so";
		syntax = "<new-ident>";
	}

	CmdResult Handle(const char** parameters, int pcnt, userrec *user)
	{
		for(unsigned int x = 0; x < strlen(parameters[0]); x++)
		{
			if(((parameters[0][x] >= 'A') && (parameters[0][x] <= '}')) || strchr(".-0123456789", parameters[0][x]))
				continue;
			
			user->WriteServ("NOTICE %s :*** Invalid characters in ident", user->nick);
			return CMD_FAILURE;
		}

		user->ChangeIdent(parameters[0]);
		ServerInstance->WriteOpers("%s used SETIDENT to change their ident to '%s'", user->nick, user->ident);

		return CMD_SUCCESS;
	}
};


class ModuleSetIdent : public Module
{
	cmd_setident*	mycommand;
	
 public:
	ModuleSetIdent(InspIRCd* Me) : Module::Module(Me)
	{
		
		mycommand = new cmd_setident(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSetIdent()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR,API_VERSION);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSetIdentFactory : public ModuleFactory
{
 public:
	ModuleSetIdentFactory()
	{
	}
	
	~ModuleSetIdentFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSetIdent(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetIdentFactory;
}

