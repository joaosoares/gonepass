#include "gonepass.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static void handle_copy_button(GtkButton * button, gpointer data) {
    GtkEntry * value_field = GTK_ENTRY(data);
    GdkAtom atom = gdk_atom_intern_static_string("CLIPBOARD");

    const char * value = gtk_entry_get_text(value_field);
    GtkClipboard * clipboard = gtk_clipboard_get(atom);
    gtk_clipboard_set_text(clipboard, value, -1);
    gtk_clipboard_store(clipboard);
}

static void handle_reveal_button(GtkButton * button, gpointer data) {
    GtkEntry * value_field = GTK_ENTRY(data);
    gboolean visible = gtk_entry_get_visibility(value_field);
    visible = !visible;
    gtk_entry_set_visibility(value_field, visible);
    if(visible)
        gtk_button_set_label(button, "_Hide");
    else
        gtk_button_set_label(button, "_Reveal");
}

static void handle_refresh_button(GtkButton * button, gpointer data) {
  char * secret = (char *) data; // get the secret
}

static int get_totp_secret(const char * totp_uri, char ** secret, size_t * secretlen) {
  // objects required for uriparser
  UriUriA uri; // the uri object
  UriQueryListA * queryList; // the query
  UriParserStateA state;
  int itemCount;
  char * b32_secret;
  int rc;

  // add uri to state object and parse key/value pairs
  state.uri = &uri;
  uriParseUriA(&state, totp_uri);
  uriDissectQueryMallocA(&queryList, &itemCount, uri.query.first, uri.query.afterLast);
  /* iterate through the linked list until we find the secret param */
  while (queryList != NULL) {
    if (!strcmp(queryList->key, "secret")) { // strcmp returns 0 if match
      b32_secret = strdup(queryList->value);
    }
      queryList = queryList->next;
  }
  oath_init();
  rc = oath_base32_decode(b32_secret, strlen(b32_secret), secret, secretlen);
  oath_done();
  return rc;
}

static void process_single_field(
        GtkWidget * container,
        int row_index,
        const char * label,
        const char * value,
        int is_password,
        int is_totp) {

    GtkWidget * label_widget = gtk_label_new(label);
    gtk_widget_set_halign(GTK_WIDGET(label_widget), GTK_ALIGN_END);
    gtk_widget_set_margin_end(GTK_WIDGET(label_widget), 5);
    GtkWidget * value_widget = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(value_widget), value);
    gtk_widget_set_hexpand(GTK_WIDGET(value_widget), TRUE);

    g_object_set(G_OBJECT(value_widget),
        "editable", FALSE,
        NULL
    );

    GtkWidget * copy_button = NULL, *reveal_button = NULL, *refresh_button = NULL;

    if(is_totp) {
      struct TotpStruct {
        GtkWidget * field;
        char * secret;
        size_t secretlen;
      } totp;
      totp.field = gtk_entry_new();

      char * secret; size_t secretlen; char otp[10]; time_t now;
      get_totp_secret(value, &totp.secret, &totp.secretlen);
      time(&now);
      oath_init();    /* Now let's use oath to generate the totp token */
      oath_totp_generate(totp.secret, totp.secretlen, now, OATH_TOTP_DEFAULT_TIME_STEP_SIZE, OATH_TOTP_DEFAULT_START_TIME, 6, otp);
      oath_done();

      gtk_entry_set_text(GTK_ENTRY(value_widget), otp);

      copy_button = gtk_button_new_with_mnemonic("_Copy");
      g_signal_connect(G_OBJECT(copy_button), "clicked",
          G_CALLBACK(handle_copy_button), value_widget);

      refresh_button = gtk_button_new_with_mnemonic("_Refresh");
      g_signal_connect(G_OBJECT(refresh_button), "clicked",
          G_CALLBACK(handle_refresh_button), value_widget);
    }
    else {

    }

    if(is_password) {
        gtk_entry_set_visibility(GTK_ENTRY(value_widget), FALSE);
        copy_button = gtk_button_new_with_mnemonic("_Copy");
        g_signal_connect(G_OBJECT(copy_button), "clicked",
            G_CALLBACK(handle_copy_button), value_widget);
        reveal_button = gtk_button_new_with_mnemonic("_Reveal");
        g_signal_connect(G_OBJECT(reveal_button), "clicked",
            G_CALLBACK(handle_reveal_button), value_widget);
    }

    gtk_grid_attach(GTK_GRID(container), label_widget, 0, row_index, 1, 1);
    if(value_widget) {
      gtk_grid_attach(GTK_GRID(container), value_widget, 1, row_index, copy_button == NULL ? 3 : 1, 1);
    }
    if(copy_button) {
        gtk_grid_attach(GTK_GRID(container), copy_button, 2, row_index, 1, 1);
    }
    if(reveal_button) {
      gtk_grid_attach(GTK_GRID(container), reveal_button, 3, row_index, 1, 1);
    }
    if(refresh_button) {
      gtk_grid_attach(GTK_GRID(container), refresh_button, 3, row_index, 1, 1);
    }
}

static int process_fields_raw(json_t * input, GtkWidget * container, int * index,
    char * value_key, char * type_key, char * title_key) {

    int row_index = *index, tmp_index;
    json_t * row_obj;
    json_array_foreach(input, tmp_index, row_obj) {
        char * item_value, *designation, *item_type;
        GtkTreeIter cur_item;
        if(json_unpack(row_obj, "{s:s s:s s:s}",
            value_key, &item_value,
            title_key, &designation,
            type_key, &item_type
        ) == -1)
            continue;

        if(strlen(item_value) == 0)
            continue;
        int is_totp = !strncmp(item_value, "otpauth://", 10); // 10 is the size of the latter array

        printf("TOTP?  %d,  VALUE %s\n", is_totp, item_value);
        int is_password = !is_totp && ( strcmp(item_type, "P") == 0 || strcmp(item_type, "concealed") == 0 );
        process_single_field(container, row_index, designation, item_value, is_password, is_totp);
        row_index++;
    }
    *index = row_index;
    return 0;
}

static int process_section(json_t * input, GtkWidget * container, int * index) {
    char * section_title;
    json_t * section_fields;
    int fields_rc, title_rc, rc;

    // printf(json_dumps(input, JSON_INDENT(2)));
    // only get one key/value at a time to avoid segmentation fault
    fields_rc = json_unpack(input, "{ s:o }",
        "fields", &section_fields
    );
    /* if title_rc != 0, the section has no title */
    title_rc = json_unpack(input, "{ s:s }",
        "title", &section_title
    );

    int row_index = *index;
    GtkWidget * title_label = gtk_label_new( (!title_rc && (strlen(section_title) > 0)) ? section_title : "Details");
    gtk_grid_attach(GTK_GRID(container), title_label, 0, row_index++, 4, 1);
    rc = process_fields_raw(section_fields, container, &row_index, "v", "k", "t");
    if(rc != 0)
        return rc;
    *index = row_index;
    return 0;
}

static int process_fields(json_t * input, GtkWidget * container, int * index) {
    int row_index = *index;
    GtkWidget * title_label = gtk_label_new("Details");
    gtk_grid_attach(GTK_GRID(container), title_label, 0, row_index++, 4, 1);

    int rc = process_fields_raw(input, container, &row_index, "value", "type", "designation");
    *index = row_index;
    return rc;
}

int process_entries(json_t * input, GtkWidget * container) {
    char * notes_plain = NULL, *password = NULL;
    json_t * fields = NULL, *sections = NULL, *urls = NULL;
    // printf(json_dumps(input, JSON_INDENT(2)));
    json_unpack(input,
        "{ s?:o s?o s?s s?o s?s }",
        "fields", &fields,
        "sections", &sections,
        "password", &password,
        "URLs", &urls,
        "notesPlain", &notes_plain
    );
    int rc = 0;

    GtkWidget * details_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(details_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(details_grid), 3);
    gtk_widget_set_margin_end(GTK_WIDGET(details_grid), 5);
    gtk_widget_set_margin_start(GTK_WIDGET(details_grid), 5);
    gtk_box_pack_start(GTK_BOX(container), details_grid, TRUE, TRUE, 2);
    int row_index = 0;

    if(password) {
        GtkWidget * notes_label = gtk_label_new("Password");
        gtk_grid_attach(GTK_GRID(details_grid), notes_label, 0, row_index++, 4, 1);
        process_single_field(details_grid, row_index++, "password", password, 1, 0);
    }

    if(fields)
        rc = process_fields(fields, details_grid, &row_index);
    if(sections) {
        json_t * section_obj;
        int section_index;

        json_array_foreach(sections, section_index, section_obj) {
            rc = process_section(section_obj, details_grid, &row_index);
            if(rc != 0)
                break;
        }
    }

    if(rc != 0)
        return rc;

    if(notes_plain) {
        GtkWidget * notes_label = gtk_label_new("Notes");
        gtk_grid_attach(GTK_GRID(details_grid), notes_label, 0, row_index++, 4, 1);

        GtkWidget * notes_field = gtk_text_view_new();
        gtk_widget_set_hexpand(GTK_WIDGET(notes_field), TRUE);
        gtk_widget_set_vexpand(GTK_WIDGET(notes_field), TRUE);
        gtk_grid_attach(GTK_GRID(details_grid), notes_field, 0, row_index++, 4, 1);
        GtkTextBuffer * buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(notes_field));
        gtk_text_buffer_set_text(buffer, notes_plain, -1);
        g_object_set(G_OBJECT(notes_field),
            "editable", FALSE,
            NULL
        );
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(notes_field), GTK_WRAP_WORD);
    }

    if(urls) {
        json_t * url_obj;
        int url_index;

        GtkWidget * notes_label = gtk_label_new("URLs");
        gtk_grid_attach(GTK_GRID(details_grid), notes_label, 0, row_index++, 4, 1);

        json_array_foreach(urls, url_index, url_obj) {
            /* url and label for the link button */
            const char * url;
            char label[44];
            json_unpack(url_obj, "{s:s}", "url", &url);
            /* copy first 40 chars of the url */
            strncpy(label, url, 40);
            /* ellipsis and null terminator */
            label[40] = '.'; label[41] = '.'; label[42] = '.'; label[43] = 0;
            GtkWidget * link_button = gtk_link_button_new_with_label(url, label);
            gtk_grid_attach(GTK_GRID(details_grid), link_button, 0, row_index++, 4, 1);
        }
    }

    return 0;
}
