/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

class ModuleSeeNicks final
	: public Module
{
public:
	ModuleSeeNicks()
		: Module(VF_VENDOR, "Sends a notice to snomasks n (local) and N (remote) when a user changes their nickname.")
	{
	}

	void init() override
	{
		ServerInstance->SNO.EnableSnomask('n', "NICK");
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		ServerInstance->SNO.WriteToSnoMask(IS_LOCAL(user) ? 'n' : 'N', "User {} changed their nickname to {}", oldnick, user->nick);
	}
};

MODULE_INIT(ModuleSeeNicks)
