#include "ui/PreferencesDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

namespace hungryeditor {

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));

    fontFamily_ = new QFontComboBox(this);
    fontFamily_->setFontFilters(QFontComboBox::MonospacedFonts);

    fontSize_ = new QSpinBox(this);
    fontSize_->setRange(6, 72);

    tabWidth_ = new QSpinBox(this);
    tabWidth_->setRange(1, 16);

    wordWrap_ = new QCheckBox(tr("Wrap long lines"), this);

    auto* form = new QFormLayout();
    form->addRow(tr("Editor font:"), fontFamily_);
    form->addRow(tr("Font size:"), fontSize_);
    form->addRow(tr("Tab width:"), tabWidth_);
    form->addRow(QString(), wordWrap_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void PreferencesDialog::setPreferences(const Preferences& preferences)
{
    if (!preferences.fontFamily.isEmpty()) {
        fontFamily_->setCurrentFont(QFont(preferences.fontFamily));
    }
    fontSize_->setValue(preferences.fontSize);
    tabWidth_->setValue(preferences.tabWidth);
    wordWrap_->setChecked(preferences.wordWrap);
}

Preferences PreferencesDialog::preferences() const
{
    Preferences preferences;
    preferences.fontFamily = fontFamily_->currentFont().family();
    preferences.fontSize = fontSize_->value();
    preferences.tabWidth = tabWidth_->value();
    preferences.wordWrap = wordWrap_->isChecked();
    return preferences;
}

} // namespace hungryeditor
