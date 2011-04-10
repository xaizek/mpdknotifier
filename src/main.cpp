//
// Copyright (C) 2009 Jakub Horák <kubahorak@gmail.com>
// Copyright (C) 2010-2011 xaizek <xaizek@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the
// Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
//

#include <KAboutData>
#include <KApplication>
#include <KCmdLineArgs>

#include <string.h>

#include "notifier.h"

bool g_debugMode;

int main(int argc, char** argv)
{
    KAboutData aboutData("mpdknotifier",
                         0,
                         ki18n("MPD KDE Notifier"),
                         "2.0.2",
                         ki18n("A notification application that informs you "
                               "about currently played song."),
                         KAboutData::KAboutData::License_GPL_V2);
    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions options;
    options.add("d").add("debug", ki18n("Enable debug mode"));
    KCmdLineArgs::addCmdLineOptions(options);

    KApplication app;

    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    g_debugMode = args->isSet("debug");

    Notifier notif;

    return (app.exec());
}
