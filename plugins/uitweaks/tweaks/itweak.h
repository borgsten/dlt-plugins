#pragma once

/* A single change to the viewer UI that can be undone. */
class ITweak {
 public:
    ITweak() = default;
    ITweak(const ITweak&) = delete;
    ITweak& operator=(const ITweak&) = delete;
    ITweak(ITweak&&) = delete;
    ITweak& operator=(ITweak&&) = delete;
    virtual ~ITweak() = default;

    virtual void apply() = 0;
    virtual void revert() = 0;
};
