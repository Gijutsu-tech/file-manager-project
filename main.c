#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <termios.h>
#include <limits.h>

struct info
{
    int size;
    char type[16];
    char name[64];
    time_t mod_time;
};

struct termios old_props, raw_props;

typedef struct
{
    struct info *entry_list;
    int list_length;
    int max_len;
    int selected_entry;
    char current_entry_name[CHAR_MAX];
} app_state;

int retrieve_entries(DIR *dirp, struct dirent *entry, app_state *app);
int handle_interface(app_state *app);
int redraw_list(app_state *app);
int rm(char *entry_name, char *type);
int prompt(app_state *app, char *header, char *input);

int main()
{
    printf("\033[?1049h");

    DIR *dirp;
    struct dirent *entry;
    app_state app;

    tcgetattr(STDIN_FILENO, &old_props);
    raw_props = old_props;
    raw_props.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_props);

    while (1)
    {
        app.max_len, app.list_length = 0;

        if (strcmp(app.current_entry_name, ""))
        {
            chdir(app.current_entry_name);
        }

        dirp = opendir(".");

        if (dirp == NULL)
        {
            printf("Failed to open the directory\n");
            return 1;
        }

        while ((entry = readdir(dirp)) != NULL)
        {
            int entry_len = strlen(entry->d_name);
            if (entry_len > app.max_len)
                app.max_len = entry_len;
            app.list_length++;
        }
        rewinddir(dirp);

        app.entry_list = malloc(app.list_length * sizeof(struct info));

        retrieve_entries(dirp, entry, &app);

        int exit = handle_interface(&app);

        free(app.entry_list);
        app.entry_list = NULL;
        if (closedir(dirp) != 0)
        {
            printf("Error closing the directory\n");
            return 1;
        }

        if (exit)
            break;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_props);

    printf("\033[?1049l");
    return 0;
}

int retrieve_entries(DIR *dirp, struct dirent *entry, app_state *app)
{
    struct stat buff;
    struct info info;
    int entry_index = 0;

    while ((entry = readdir(dirp)) != NULL)
    {
        char str_size[16];
        stat(entry->d_name, &buff);
        if (S_ISREG(buff.st_mode))
        {
            strcpy(info.type, "FILE");
        }
        else if (S_ISDIR(buff.st_mode))
        {
            strcpy(info.type, "FOLDER");
        }
        else
        {
            strcpy(info.type, "OTHER");
        }
        strcpy(info.name, entry->d_name);
        info.mod_time = buff.st_mtime;
        info.size = buff.st_size;

        snprintf(str_size, sizeof(str_size), "%d", info.size);

        app->entry_list[entry_index] = info;
        entry_index++;
    }

    return 0;
}

int handle_interface(app_state *app)
{
    app->selected_entry = 0;
    int cursor_pos = 0;

    printf("\033[H\033[2J");
    redraw_list(app);
    printf("\033[%dA>", (app->list_length));

    while (1)
    {
        char key;

        if ((key = getchar()) == '\033')
        {
            if ((key = getchar()) == '[')
            {

                key = getchar();
                if (key == 'A' && app->selected_entry > 0)
                {
                    if (cursor_pos > 0)
                    {
                        cursor_pos--;
                        app->selected_entry--;
                    }
                    printf("\033[H");
                    redraw_list(app);
                    printf("\033[H\033[%dB>", cursor_pos);

                    continue;
                }
                else if (key == 'B')
                {
                    if (cursor_pos < (app->list_length))
                    {
                        cursor_pos++;
                        app->selected_entry++;
                    }
                    printf("\033[H");
                    redraw_list(app);
                    printf("\033[H\033[%dB>", cursor_pos);

                    continue;
                }
                else
                {
                    continue;
                }
            }
        }
        else
        {

            if (key == '\n')
            {
                if (cursor_pos == app->list_length)
                {
                    return 1;
                }
                
                strcpy(app->current_entry_name, app->entry_list[app->selected_entry].name);
                app->selected_entry = 0;
                printf("\033[2J\033[1D");
                break;
            }
            else if (key == 'd')
            {
                rm(app->entry_list[app->selected_entry].name, app->entry_list[app->selected_entry].type);
                break;
            }
            else if (key == 'n')
            {
                char new_dirname[128];
                prompt(app, "Enter directory name: ", new_dirname);
                if (mkdir(new_dirname, 0777) == -1)
                {
                    perror("Error creating directory");
                }
                break;
            }
        }
    }

    return 0;
}

int redraw_list(app_state *app)
{
    char str_size[16];

    for (int i = 0; i < app->list_length; i++)
    {
        struct info *entries = &app->entry_list[i];
        snprintf(str_size, sizeof(str_size), "%d", entries->size);
        printf(" [%-6s] %-*s\t%s\t%s", entries->type, app->max_len, entries->name, !strcmp(entries->type, "FOLDER") ? "-" : str_size, ctime(&(entries->mod_time)));
    }

    printf(" EXIT\033[5D");
}

int rm(char *entry_name, char *type)
{
    if (!strcmp(type, "FILE"))
    {
        unlink(entry_name);
    }
    else
    {
        DIR *dir_ = opendir(entry_name);
        struct dirent *folder;
        struct stat buff_;
        char path[512];

        while ((folder = readdir(dir_)) != NULL)
        {
            if (!strcmp(folder->d_name, ".") || !strcmp(folder->d_name, ".."))
                continue;

            snprintf(path, sizeof(path), "%s/%s", entry_name, folder->d_name);
            stat(path, &buff_);

            if (S_ISREG(buff_.st_mode))
                rm(path, "FILE");
            else
                rm(path, "FOLDER");
        }

        closedir(dir_);
        rmdir(entry_name); // now empty, can delete
    }

    return 0;
}

int prompt(app_state *app, char *header, char *input)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &old_props);
    printf("\033[H\033[2J");
    redraw_list(app);
    printf("\n%s", header);
    if (fgets(input, sizeof(input), stdin))
    {
        input[strcspn(input, "\n")] = '\0';
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_props);

    return 0;
}