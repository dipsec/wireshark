/* simple_statistics_dialog.cpp
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "simple_statistics_dialog.h"

#include "file.h"

#include "epan/stat_tap_ui.h"

#include <QMessageBox>
#include <QTreeWidget>

#include "wireshark_application.h"

// To do:
// - Hide rows with zero counts.

static QHash<const QString, new_stat_tap_ui *> cfg_str_to_stu_;

extern "C" {
static void
simple_stat_init(const char *args, void*) {
    QStringList args_l = QString(args).split(',');
    if (args_l.length() > 1) {
        QString simple_stat = QString("%1,%2").arg(args_l[0]).arg(args_l[1]);
        QString filter;
        if (args_l.length() > 2) {
            filter = QStringList(args_l.mid(2)).join(",");
        }
        wsApp->emitTapParameterSignal(simple_stat, filter, NULL);
    }
}
}

void register_simple_stat_tables(gpointer data, gpointer) {
    new_stat_tap_ui *stu = (new_stat_tap_ui*)data;

    cfg_str_to_stu_[stu->cli_string] = stu;
    TapParameterDialog::registerDialog(
                stu->title,
                stu->cli_string,
                stu->group,
                simple_stat_init,
                SimpleStatisticsDialog::createSimpleStatisticsDialog);
}

enum {
    simple_row_type_ = 1000
};

class SimpleStatisticsTreeWidgetItem : public QTreeWidgetItem
{
public:
    SimpleStatisticsTreeWidgetItem(QTreeWidget *parent, int num_fields, const stat_tap_table_item_type *fields) :
        QTreeWidgetItem (parent, simple_row_type_),
        num_fields_(num_fields),
        fields_(fields)
    {
    }
    void draw() {
        for (int i = 0; i < num_fields_ && i < treeWidget()->columnCount(); i++) {
            switch (fields_[i].type) {
            case TABLE_ITEM_UINT:
                setText(i, QString::number(fields_[i].value.uint_value));
                break;
            case TABLE_ITEM_INT:
                setText(i, QString::number(fields_[i].value.int_value));
                break;
            case TABLE_ITEM_STRING:
                setText(i, fields_[i].value.string_value);
                break;
            case TABLE_ITEM_FLOAT:
                setText(i, QString::number(fields_[i].value.float_value, 'f', 6));
                break;
            case TABLE_ITEM_ENUM:
                setText(i, QString::number(fields_[i].value.enum_value));
                break;
            default:
                break;
            }
        }
    }
    bool operator< (const QTreeWidgetItem &other) const
    {
        int col = treeWidget()->sortColumn();
        if (other.type() != simple_row_type_ || col >= num_fields_) {
            return QTreeWidgetItem::operator< (other);
        }
        const SimpleStatisticsTreeWidgetItem *other_row = static_cast<const SimpleStatisticsTreeWidgetItem *>(&other);
        switch (fields_[col].type) {
        case TABLE_ITEM_UINT:
            return fields_[col].value.uint_value < other_row->fields_[col].value.uint_value;
        case TABLE_ITEM_INT:
            return fields_[col].value.int_value < other_row->fields_[col].value.int_value;
        case TABLE_ITEM_STRING:
            return g_strcmp0(fields_[col].value.string_value, other_row->fields_[col].value.string_value) < 0;
        case TABLE_ITEM_FLOAT:
            return fields_[col].value.float_value < other_row->fields_[col].value.float_value;
        case TABLE_ITEM_ENUM:
            return fields_[col].value.enum_value < other_row->fields_[col].value.enum_value;
        default:
            break;
        }

        return QTreeWidgetItem::operator< (other);
    }
    QList<QVariant> rowData() {
        QList<QVariant> row_data;

        for (int i = 0; i < num_fields_ && i < columnCount(); i++) {
            switch (fields_[i].type) {
            case TABLE_ITEM_UINT:
                row_data << fields_[i].value.uint_value;
                break;
            case TABLE_ITEM_INT:
                row_data << fields_[i].value.int_value;
                break;
            case TABLE_ITEM_STRING:
                row_data << fields_[i].value.string_value;
                break;
            case TABLE_ITEM_FLOAT:
                row_data << fields_[i].value.float_value;
                break;
            case TABLE_ITEM_ENUM:
                row_data << fields_[i].value.enum_value;
                break;
            default:
                break;
            }
        }

        return row_data;
    }

private:
    const int num_fields_;
    const stat_tap_table_item_type *fields_;
};

SimpleStatisticsDialog::SimpleStatisticsDialog(QWidget &parent, CaptureFile &cf, struct _new_stat_tap_ui *stu, const QString filter, int help_topic) :
    TapParameterDialog(parent, cf, help_topic),
    stu_(stu)
{
    setWindowSubtitle(stu_->title);

    QStringList header_labels;
    for (int col = 0; col < (int) stu_->nfields; col++) {
        header_labels << stu_->fields[col].column_name;
    }
    statsTreeWidget()->setHeaderLabels(header_labels);

    for (int col = 0; col < (int) stu_->nfields; col++) {
        if (stu_->fields[col].align == TAP_ALIGN_RIGHT) {
            statsTreeWidget()->headerItem()->setTextAlignment(col, Qt::AlignRight);
        }
    }

    setDisplayFilter(filter);
}

TapParameterDialog *SimpleStatisticsDialog::createSimpleStatisticsDialog(QWidget &parent, const QString cfg_str, const QString filter, CaptureFile &cf)
{
    if (!cfg_str_to_stu_.contains(cfg_str)) {
        // XXX MessageBox?
        return NULL;
    }

    new_stat_tap_ui *stu = cfg_str_to_stu_[cfg_str];

    return new SimpleStatisticsDialog(parent, cf, stu, filter);
}

void SimpleStatisticsDialog::addSimpleStatisticsTable(const _stat_tap_table *st_table)
{
    // Hierarchy:
    // - tables (GTK+ UI only supports one currently)
    //   - elements (rows?)
    //     - fields (columns?)
    // For multiple table support we might want to add them as subtrees, with
    // the top-level tree item text set to the column labels for that table.

    for (guint element = 0; element < st_table->num_elements; element++) {
        stat_tap_table_item_type* fields = new_stat_tap_get_field_data(st_table, element, 0);
        new SimpleStatisticsTreeWidgetItem(statsTreeWidget(), st_table->num_fields, fields);
    }

}

void SimpleStatisticsDialog::tapReset(void *sd_ptr)
{
    new_stat_data_t *sd = (new_stat_data_t*) sd_ptr;
    SimpleStatisticsDialog *ss_dlg = static_cast<SimpleStatisticsDialog *>(sd->user_data);
    if (!ss_dlg) return;

    reset_stat_table(sd->new_stat_tap_data, NULL, NULL);
    ss_dlg->statsTreeWidget()->clear();

    guint table_index = 0;
    new_stat_tap_table* st_table = g_array_index(sd->new_stat_tap_data->tables, new_stat_tap_table*, table_index);
    ss_dlg->addSimpleStatisticsTable(st_table);
}

void SimpleStatisticsDialog::tapDraw(void *sd_ptr)
{
    new_stat_data_t *sd = (new_stat_data_t*) sd_ptr;
    SimpleStatisticsDialog *ss_dlg = static_cast<SimpleStatisticsDialog *>(sd->user_data);
    if (!ss_dlg) return;

    QTreeWidgetItemIterator it(ss_dlg->statsTreeWidget());
    while (*it) {
        if ((*it)->type() == simple_row_type_) {
            SimpleStatisticsTreeWidgetItem *ss_ti = static_cast<SimpleStatisticsTreeWidgetItem *>((*it));
            ss_ti->draw();
        }
        ++it;
    }

    for (int i = 0; i < ss_dlg->statsTreeWidget()->columnCount() - 1; i++) {
        ss_dlg->statsTreeWidget()->resizeColumnToContents(i);
    }
}

void SimpleStatisticsDialog::fillTree()
{
    new_stat_data_t stat_data;
    stat_data.new_stat_tap_data = stu_;
    stat_data.user_data = this;

    stu_->stat_tap_init_cb(stu_, NULL, NULL);

    GString *error_string = register_tap_listener(stu_->tap_name,
                          &stat_data,
                          displayFilter(),
                          0,
                          tapReset,
                          stu_->packet_func,
                          tapDraw);
    if (error_string) {
        QMessageBox::critical(this, tr("Failed to attach to tap \"%1\"").arg(stu_->tap_name),
                             error_string->str);
        g_string_free(error_string, TRUE);
        free_stat_table(stu_, NULL, NULL);
        reject();
    }

    cap_file_.retapPackets();

    tapDraw(&stat_data);

    remove_tap_listener(&stat_data);
    free_stat_table(stu_, NULL, NULL);
}

/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
