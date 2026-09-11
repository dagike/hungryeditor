#pragma once

#include <QDialog>

#include "io/Preferences.h"

class QCheckBox;
class QFontComboBox;
class QSpinBox;

namespace hungryeditor {

/// A small settings dialog: editor font, tab width, word wrap and the opt-in
/// local crash reporter. Reads and writes a plain Preferences value —
/// MainWindow owns persisting it.
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

    void setPreferences(const Preferences& preferences);
    Preferences preferences() const;

private:
    QFontComboBox* fontFamily_ = nullptr;
    QSpinBox* fontSize_ = nullptr;
    QSpinBox* tabWidth_ = nullptr;
    QCheckBox* wordWrap_ = nullptr;
    QCheckBox* crashReporting_ = nullptr;
};

} // namespace hungryeditor
