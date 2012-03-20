//============================================================================
// Name        : NovaCLI.h
// Copyright   : DataSoft Corporation 2011-2012
//	Nova is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   Nova is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with Nova.  If not, see <http://www.gnu.org/licenses/>.
// Description : Command line interface for Nova
//============================================================================

#ifndef NOVACLI_H_
#define NOVACLI_H_

#include "messages/RequestMessage.h"

namespace NovaCLI
{
	void StartNovaWrapper();
	void StartHaystackWrapper();
	void StopNovaWrapper();
	void StopHaystackWrapper();
	void PrintSuspectList(enum SuspectListType listType);


	void PrintUsage();
}


#endif /* NOVACLI_H_ */
