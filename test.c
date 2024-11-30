#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char name[256];           // File or directory name
    int is_directory;         // 1 if directory, 0 if file
    off_t size;               // File size (only for files)
    time_t modified_time;     // Last modified time (only for files)
} FileDirEntry;

void exp_read_dir(const char* path) {
    DIR* dirFile = opendir(path);
    struct dirent* hFile;

    if (!dirFile) {
        perror("Could not open directory");
        return;
    }

    printf("Contents of directory '%s':\n", path);

    while ((hFile = readdir(dirFile)) != NULL) {
        // Skip "." and ".."
        if (!strcmp(hFile->d_name, ".") || !strcmp(hFile->d_name, ".."))
            continue;

        // Construct the full path
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, hFile->d_name);

        struct stat fileStat;
        if (stat(full_path, &fileStat) == -1) {
            perror("Stat failed");
            continue;
        }

        // Fill the custom struct
        FileDirEntry entry;
        snprintf(entry.name, sizeof(entry.name), "%s", hFile->d_name);

        if (S_ISDIR(fileStat.st_mode)) {
            entry.is_directory = 1;
            entry.size = 0; // Directories don't have meaningful sizes
            entry.modified_time = fileStat.st_mtime;

            printf("[DIR]  %s (Modified: %s)\n",
                   entry.name,
                   ctime(&entry.modified_time));
        } else if (S_ISREG(fileStat.st_mode)) {
            entry.is_directory = 0;
            entry.size = fileStat.st_size;
            entry.modified_time = fileStat.st_mtime;

            // Print formatted information
            char mod_time[20];
            strftime(mod_time, sizeof(mod_time), "%Y-%m-%d %H:%M:%S", localtime(&entry.modified_time));

            printf("[FILE] %s (Size: %lld bytes, Modified: %s)\n",
                   entry.name,
                   (long long)entry.size,
                   mod_time);
        } else {
            printf("[OTHER] %s\n", entry.name);
        }
    }

    closedir(dirFile);
}

int main(int argc, char* argv[]) {
    const char* path = argc > 1 ? argv[1] : "."; // Default to current directory
    exp_read_dir(path);
    return 0;
}
