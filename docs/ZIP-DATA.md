# ZIP coordinate data

Source: [GeoNames US postal-code download](https://download.geonames.org/export/zip/US.zip),
retrieved October 27, 2026. Attribution: [GeoNames](https://www.geonames.org/),
licensed under [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).
GeoNames supplies the data without guarantees of accuracy or completeness.

The compiled table has 41,488 distinct five-digit ZIP codes. The generator
retains the first place for each ZIP, sorts numerically while preserving leading
zeros at the string interface, and rounds coordinates to 0.00001 degrees. Each
record contains a numeric ZIP and two signed integer coordinates (12 bytes).
It contains approximate postal-area coordinates, not household or device positions.

Archive SHA256: `4973FDF8003AE16E572D68410995DCEA150C5A71284EC84ADBCFEF7C5ABB49A7`.
The checksum is also recorded in the generated `src/ZipCodeData.h` header.

Regenerate deliberately with `./tools/update-zip-codes.ps1`, or pass `-Archive`
with an existing GeoNames US ZIP archive. Review the resulting count, checksum
and table diff before building. Builds never download or regenerate this data.

Missing ZIPs use [Zippopotam.us](https://api.zippopotam.us/) during setup only.
Its data also derives from GeoNames. The firmware uses the first returned
place's validated latitude/longitude; failures use the owner's explicit fallback
35.84562, -86.39027. Timezone remains the separately configured browser/override
setting, rather than being inferred from the ZIP.
