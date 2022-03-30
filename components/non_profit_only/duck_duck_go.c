// DuckDuckGo queries: Must be non-profit. Require attribution where they are displayed.
// Free IP geolocation, and more accurate than others I've tried. Although it's a
// JSON query, the answer is in natural language with an embedded URL, and must be
// pattern scanned.
// https://duckduckgo.com/?q=what+is+my+ip&format=json&kl=us-en
//
// { "Answer":"Your IP address is 24.130.55.121 in <a href=\"https://duckduckgo.com/?q=Berkeley%2C%20California%2C%20United%20States%20(94708)&iar=maps_maps\">Berkeley, California, United States (94708)</a>" }
// We get the IP address from other APIs that do not require attribution, so ignore
// that part, and just get the geolocation.
// Regex to capture the geolocation: "<a [^>]+>([^<]+)</a>"
