#pragma once

#include <mbgl/storage/resource.hpp>
#include <mbgl/storage/offline.hpp>
#include <mbgl/util/tile_server_options.hpp>
#include <mbgl/util/exception.hpp>
#include <mbgl/util/constants.hpp>
#include <mbgl/util/mapbox.hpp>
#include <mbgl/util/expected.hpp>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <optional>

namespace mapbox {
namespace sqlite {
class Database;
class Statement;
class Query;
class Exception;
} // namespace sqlite
} // namespace mapbox

namespace mbgl {

class Response;
class TileID;

namespace util {
struct IOException;
} // namespace util

struct MapboxTileLimitExceededException :  util::Exception {
    MapboxTileLimitExceededException() : util::Exception("Mapbox tile limit exceeded") {}
};

class OfflineDatabase {
public:
    OfflineDatabase(std::string path, const TileServerOptions& options);
    ~OfflineDatabase();

    void changePath(const std::string&);
    std::exception_ptr resetDatabase();

    std::optional<Response> get(const Resource&);

    // Return value is (inserted, stored size)
    std::pair<bool, uint64_t> put(const Resource&, const Response&);

    // Force Mapbox GL Native to revalidate tiles stored in the ambient
    // cache with the tile server before using them, making sure they
    // are the latest version. This is more efficient than cleaning the
    // cache because if the tile is considered valid after the server
    // lookup, it will not get downloaded again.
    std::exception_ptr invalidateAmbientCache();

    // Clear the tile cache, freeing resources. This operation can be
    // potentially slow because it will trigger a VACUUM on SQLite,
    // forcing the database to move pages on the filesystem.
    std::exception_ptr clearAmbientCache();

    expected<OfflineRegions, std::exception_ptr> listRegions();

    expected<OfflineRegion, std::exception_ptr> createRegion(const OfflineRegionDefinition&,
                                                             const OfflineRegionMetadata&);

    expected<OfflineRegions, std::exception_ptr>
    mergeDatabase(const std::string& sideDatabasePath);

    expected<OfflineRegionMetadata, std::exception_ptr> updateMetadata(int64_t regionID, const OfflineRegionMetadata&);

    std::exception_ptr deleteRegion(OfflineRegion&&);
    std::exception_ptr invalidateRegion(int64_t regionID);

    // Return value is (response, stored size)
    std::optional<std::pair<Response, uint64_t>> getRegionResource(const Resource&);
    std::optional<int64_t> hasRegionResource(const Resource&);
    uint64_t putRegionResource(int64_t regionID, const Resource&, const Response&);
    void putRegionResources(int64_t regionID, const std::list<std::tuple<Resource, Response>>&, OfflineRegionStatus&);

    expected<OfflineRegionDefinition, std::exception_ptr> getRegionDefinition(int64_t regionID);
    expected<OfflineRegionStatus, std::exception_ptr> getRegionCompletedStatus(int64_t regionID);

    std::exception_ptr setMaximumAmbientCacheSize(uint64_t);
    void setOfflineMapboxTileCountLimit(uint64_t);
    uint64_t getOfflineMapboxTileCountLimit();
    bool offlineMapboxTileCountLimitExceeded();
    uint64_t getOfflineMapboxTileCount();
    bool exceedsOfflineMapboxTileCountLimit(const Resource&);
    void markUsedResources(int64_t regionID, const std::list<Resource>&);
    std::exception_ptr pack();
    void runPackDatabaseAutomatically(bool autopack_) { autopack = autopack_; }

    void reopenDatabaseReadOnly(bool readOnly);

private:
    class DatabaseSizeChangeStats;

    void initialize();
    void handleError(const mapbox::sqlite::Exception&, const char* action);
    void handleError(const util::IOException&, const char* action);
    void handleError(const std::runtime_error& ex, const char* action);
    void handleError(const char* action);

    void removeExisting();
    void removeOldCacheTable();
    void createSchema();
    void migrateToVersion5();
    void migrateToVersion3();
    void migrateToVersion6();
    void cleanup();
    bool disabled();
    void vacuum();
    void checkFlags();

    mapbox::sqlite::Statement& getStatement(const char *);

    std::optional<std::pair<Response, uint64_t>> getTile(const Resource::TileData&);
    std::optional<int64_t> hasTile(const Resource::TileData&);
    bool putTile(const Resource::TileData&, const Response&,
                 const std::string&, bool compressed);

    std::optional<std::pair<Response, uint64_t>> getResource(const Resource&);
    std::optional<int64_t> hasResource(const Resource&);
    bool putResource(const Resource&, const Response&,
                     const std::string&, bool compressed);

    uint64_t putRegionResourceInternal(int64_t regionID, const Resource&, const Response&);

    std::optional<std::pair<Response, uint64_t>> getInternal(const Resource&);
    std::optional<int64_t> hasInternal(const Resource&);
    std::pair<bool, uint64_t> putInternal(const Resource&, const Response&, bool evict);

    // Return value is true iff the resource was previously unused by any other regions.
    bool markUsed(int64_t regionID, const Resource&);

    std::pair<int64_t, int64_t> getCompletedResourceCountAndSize(int64_t regionID);
    std::pair<int64_t, int64_t> getCompletedTileCountAndSize(int64_t regionID);

    std::string path;
    std::unique_ptr<mapbox::sqlite::Database> db;
    std::map<const char*, const std::unique_ptr<mapbox::sqlite::Statement>> statements;

    template <class T>
    T getPragma(const char *);

    uint64_t maximumAmbientCacheSize = util::DEFAULT_MAX_CACHE_SIZE;
    uint64_t offlineMapboxTileCountLimit = util::mapbox::DEFAULT_OFFLINE_TILE_COUNT_LIMIT;

    std::optional<uint64_t> offlineMapboxTileCount;

    bool evict(uint64_t neededFreeSize, DatabaseSizeChangeStats& stats);

    TileServerOptions tileServerOptions;

    class DatabaseSizeChangeStats {
    public:
        explicit DatabaseSizeChangeStats(OfflineDatabase*);

        // Returns difference between current database size and
        // database size at the time of creation of this object.
        int64_t diff() const;

        // Returns how many bytes were released comparing to a database
        // size at the time of creation of this object.
        uint64_t bytesReleased() const;

        // Returns page size for the database.
        uint64_t pageSize() const;

    private:
        uint64_t pageSize_ = 0u;
        uint64_t pageCount_ = 0u;
        uint64_t initialSize_ = 0u;
        OfflineDatabase* db = nullptr;
    };

    friend class DatabaseSizeChangeStats;

    // Lazily initializes currentAmbientCacheSize.
    std::exception_ptr initAmbientCacheSize();
    std::optional<uint64_t> currentAmbientCacheSize;
    void updateAmbientCacheSize(DatabaseSizeChangeStats&);

    bool autopack = true;
    bool readOnly = false;
};

} // namespace mbgl
