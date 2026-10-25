# Named-reference handling

This page no longer publishes source-derived names, values, or reader details.

The runtime must preserve the distinction between absent references and resolved references. Resolution must use the owning scene's validated resource registry, retain identity for the required lifetime, and reject unknown or ambiguous mappings. Synthetic fixtures should cover null, valid, missing, and duplicate references without embedding installation data.
