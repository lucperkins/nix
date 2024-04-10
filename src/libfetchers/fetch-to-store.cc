#include "fetch-to-store.hh"
#include "fetchers.hh"
#include "cache.hh"

namespace nix {

StorePath fetchToStore(
    Store & store,
    const SourcePath & path,
    FetchMode mode,
    std::string_view name,
    ContentAddressMethod method,
    PathFilter * filter,
    RepairFlag repair)
{
    // FIXME: add an optimisation for the case where the accessor is
    // an FSInputAccessor pointing to a store path.

    auto domain = "fetchToStore";
    std::optional<fetchers::Attrs> cacheKey;

    if (!filter && path.accessor->fingerprint) {
        cacheKey = fetchers::Attrs{
            {"name", std::string{name}},
            {"fingerprint", *path.accessor->fingerprint},
            {"method", std::string{method.render()}},
            {"path", path.path.abs()}
        };
        if (auto res = fetchers::getCache()->lookupStorePath(domain, *cacheKey, store)) {
            debug("store path cache hit for '%s'", path);
            return res->storePath;
        }
    } else
        debug("source path '%s' is uncacheable", path);

    Activity act(*logger, lvlChatty, actUnknown,
        fmt(mode == FetchMode::DryRun ? "hashing '%s'" : "copying '%s' to the store", path));

    auto filter2 = filter ? *filter : defaultPathFilter;

    auto storePath =
        mode == FetchMode::DryRun
        ? store.computeStorePath(
            name, *path.accessor, path.path, method, HashAlgorithm::SHA256, {}, filter2).first
        : store.addToStore(
            name, *path.accessor, path.path, method, HashAlgorithm::SHA256, {}, filter2, repair);

    if (cacheKey && mode == FetchMode::Copy)
        fetchers::getCache()->upsert(domain, *cacheKey, store, {}, storePath);

    return storePath;
}

}
