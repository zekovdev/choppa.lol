// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/WelcomeDialog.hpp"

namespace chatterino {

WelcomeDialog::WelcomeDialog()
    : BaseWindow({BaseWindow::EnableCustomFrame, BaseWindow::ContentChrome,
                  BaseWindow::DisableLayoutSave})
{
    this->setWindowTitle("choppa.lol - Quick setup");
}

}  // namespace chatterino
