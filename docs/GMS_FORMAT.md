# GMS decoded image and packed references

Every scene archive contains one packed `GMS` resource. After outer-envelope decompression, it forms a scene data image addressed by the geometry references stored in `RMC` and `RMI` descriptors. The inner section schema is not yet decoded, so the portable layer exposes only proven, bounds-checked reference behavior.

## Reference representation

A present geometry reference is a little-endian 32-bit value:

| Bits | Meaning |
|---:|---|
| 31-30 | required tag `01` |
| 29-0 | byte offset in the paired decoded GMS image |

The tagged value `0x40000000` is a valid reference to offset zero; it is distinct from the absent optional value zero. A caller must provide the number of bytes it intends to read. The resolver verifies the tag and returns an owned view only when the complete requested range is inside the decoded image.

## Evidence

Private executable analysis shows the RMC/RMI query callback forwarding the descriptor reference to the scene database's geometry lookup. Corpus validation independently confirms that every one of the 2,801 primary references and all 201 present optional references has tag `01`, 16-byte offset alignment, and a four-byte target within its paired decoded GMS image.

The 90 decoded GMS images total 33,436,872 bytes. Their internal tables, object boundaries, relocation rules, and links to `PRM` primitives remain active research targets; the public implementation does not assign guessed semantics to those bytes.

## Validation coverage

Synthetic tests use a project-authored stored image. They cover a tagged reference to offset zero, an interior reference, exact bounded views, null and unsupported tags, zero-length requests, and end-of-image overflow. No retail GMS content is present in the repository.
