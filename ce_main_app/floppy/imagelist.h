#ifndef _IMAGELIST_H_
#define _IMAGELIST_H_

#include <map>
#include <string>
#include <vector>

#define IMAGELIST_URL       "http://joo.kie.sk/cosmosex/update/imagelist.csv"
#define IMAGELIST_LOCAL_DIR "/ce/app/"
#define IMAGELIST_LOCAL     "/ce/app/imagelist.csv"
#define IMAGE_DOWNLOAD_DIR  "/tmp/"

typedef struct {
    std::string imageName;
    std::string url;
    int         checksum;
    std::string games;
    bool        marked;
} ImageListItem;

typedef struct {
    std::string game;
    int         imageIndex;
} SearchResult;

class ImageList 
{
public:
    ImageList(void);

    bool exists(void);
    bool loadList(void);

    void search(char *part);

    int  getSearchResultsCount(void);
    void getResultByIndex(int index, char *bfr);
    void markImage(int index);
    bool getFirstMarkedImage(std::string &url, int &checksum, std::string &filename);
    void refreshList(void);

private:
    std::vector<ImageListItem>      vectorOfImages;
    std::vector<SearchResult>       vectorOfResults;

    bool isLoaded;

    void getSingleGame(std::string &games, std::string &game, size_t pos);
};

#endif

