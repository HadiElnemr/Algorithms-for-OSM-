#include "t3.h"
#include "t2.h"


#include <chrono> // for execution time

std::vector<osmium::Location> crossed_nodes;

void get_final_closed_ways(std::string &input_file_name,
                           std::vector<std::vector<osmium::object_id_type>> &final_closed_ways,
                           std::unordered_map<osmium::object_id_type, osmium::Location> &node_locations)
{
    std::vector<std::vector<osmium::object_id_type>> ways_collection;
    std::unordered_set<osmium::object_id_type> node_ids;

    // read_coastline_ways(input_file_name, ways_collection);
    // std::cout << "Coastline Ways count: " << ways_collection.size() << std::endl;
    // extract_nodes_from_ways(ways_collection, node_ids);
    // std::cout << "Nodes extracted: " << node_ids.size() << std::endl;

    auto t1 = std::chrono::high_resolution_clock::now();
    read_coastline_ways_and_node_ids(input_file_name, ways_collection, node_ids);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

    std::cout << "time for read_coastline_ways_and_node_ids: " << ms_int.count() << std::endl;

    t1 = std::chrono::high_resolution_clock::now();
    read_coastline_nodes(input_file_name, node_ids, node_locations);
    t2 = std::chrono::high_resolution_clock::now();
    ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "time for read_coastline_nodes: " << ms_int.count() << std::endl;

    std::cout << "Coastline Nodes count: " << node_locations.size() << std::endl;

    node_ids.clear();

    // Before stitching
    std::vector<std::vector<osmium::object_id_type>> closed_ways;
    std::vector<std::vector<osmium::object_id_type>> unclosed_ways;
    get_closed_ways(ways_collection, closed_ways);
    get_unclosed_ways(ways_collection, unclosed_ways);
    std::cout << "Closed Ways count (before stitching): " << closed_ways.size() << std::endl;
    std::cout << "Unclosed Ways count (before stitching): " << unclosed_ways.size() << std::endl;

    ways_collection.clear();

    // Stitching
    merge_touching_ways(unclosed_ways, final_closed_ways, false);
    std::cout << "Unclosed -> Stitched Ways count: " << final_closed_ways.size() << std::endl;

    // Merge unstitched ways with stitched ways
    for(auto way : closed_ways)
        final_closed_ways.push_back(way);
    std::cout << "\nStitched + Closed Ways final_closed_ways count: " << final_closed_ways.size() << std::endl;
    // final_closed_ways = stitched_ways;
}

// Find the bounding box for each of the closed ways
void getBoundingBox(std::vector<std::vector<osmium::object_id_type>> &closed_ways,
                    std::unordered_map<osmium::object_id_type, osmium::Location> &node_locations,
                    std::vector<std::pair<osmium::Location, osmium::Location>> &bounding_boxes)
{
    for(auto way : closed_ways){
        float min_lat = 90.0, max_lat = -90.0, min_lon = 180.0, max_lon = -180.0;
        for(auto node : way){
            osmium::Location loc = node_locations[node];
            if (loc.lat() < min_lat) min_lat = loc.lat();
            if (loc.lat() > max_lat) max_lat = loc.lat();
            if (loc.lon() < min_lon) min_lon = loc.lon();
            if (loc.lon() > max_lon) max_lon = loc.lon();
        }
        bounding_boxes.push_back(std::make_pair(osmium::Location(min_lon, min_lat), osmium::Location(max_lon, max_lat)));
    }
    bounding_boxes.shrink_to_fit();
}

float latitude(float lambda, float lambda1, float lambda2, float phi1, float phi2){
    return phi1 + (lambda - lambda1) / (lambda2 - lambda1) * (phi2 - phi1);
}

// Intersection test for Lat-Lon Edges // Can be used for calculations in the check "X.lat[at Q.lon] > Q.lat"
float latitude_crossing_X(osmium::Location Q, float lambda1, float lambda2, float phi1, float phi2){
    return phi1 + (Q.lat() - lambda1) / (lambda2 - lambda1) * (phi2 - phi1);
}

bool isPointInBox(osmium::Location &Q, osmium::Location &A, osmium::Location &B){
    return Q.lat() >= A.lat() && Q.lat() <= B.lat() && Q.lon() >= A.lon() && Q.lon() <= B.lon();
}

///////////////////////////////////////   By Darel Rex Finley
// bool isEdgeCrossedByHorizontalLine(osmium::Location Q, osmium::Location i, osmium::Location j){
//     if( (i.lat() < Q.lat && j.lat() >= Q.lat() ||   j.lat() < Q.lat() && i.lat() >= Q.lat()) // if Q is between edge lats
//         &&  (i.lon() <= Q.lon() || j.lon() <= Q.lon())) // if Q is to the right of the edge
//             return (
//             i.lon()
//             +
//             (Q.lat() - i.lat()) / (j.lat() - i.lat())
//             *
//             (j.lon() - i.lon())
//             <
//             Q.lon()
//             );
//     return false;
// }

// isEdgeCrossed by northerly ray to the North Pole
bool isEdgeCrossed(osmium::Location& Q, osmium::Location& i, osmium::Location& j){
    if( ( (i.lon() < Q.lon() && j.lon() >= Q.lon()) || (j.lon() < Q.lon() && i.lon() >= Q.lon()) ) // if Q is between edge longitudes    i    Q     j
        &&  (i.lat() >= Q.lat() || j.lat() >= Q.lat())
        && i.lat() + (j.lat() - i.lat()) / (j.lon() - i.lon()) * (Q.lon() - i.lon()) > Q.lat())
        crossed_nodes.push_back(osmium::Location(Q.lon(), i.lat() + (j.lat() - i.lat()) / (j.lon() - i.lon()) * (Q.lon() - i.lon())));

    if( ( (i.lon() < Q.lon() && j.lon() >= Q.lon()) || (j.lon() < Q.lon() && i.lon() >= Q.lon()) ) // if Q is between edge longitudes    i    Q     j
        &&  (i.lat() >= Q.lat() || j.lat() >= Q.lat())) // if the edge is above Q
            return (
            i.lat()
            +
            (j.lat() - i.lat()) / (j.lon() - i.lon())
            *
            (Q.lon() - i.lon())
            >
            Q.lat()
            );
    return false;
}

// Testing
// bool isBoundingBoxCrossed(osmium::Location &Q, std::pair<osmium::Location, osmium::Location> &bounding_box){
//     osmium::Location A = bounding_box.first;                    //  ......B                p4 ...... p3
//     osmium::Location B = bounding_box.second;                   //  A......                p1 ...... p2
//     osmium::Location p1 = osmium::Location(A.lon(), A.lat());
//     osmium::Location p2 = osmium::Location(B.lon(), A.lat());
//     osmium::Location p3 = osmium::Location(B.lon(), B.lat());
//     osmium::Location p4 = osmium::Location(A.lon(), B.lat());
//     return isEdgeCrossed(Q, p1, p2) ^ isEdgeCrossed(Q, p2, p3) ^ isEdgeCrossed(Q, p3, p4) ^ isEdgeCrossed(Q, p4, p1);
// }

bool isPointInPolygon(osmium::Location &Q,
                    std::vector<osmium::object_id_type> &way,
                    std::unordered_map<osmium::object_id_type, osmium::Location> &node_locations) {

//   auto j = way.size()-1;
  auto j = 0;
  bool parity_odd = 0;

    for(std::size_t i = 1; i < way.size(); i++) {
        parity_odd ^= isEdgeCrossed(Q, node_locations[way[i]], node_locations[way[j]]);
        j = i;
}
  return parity_odd;
}

bool isWater(osmium::Location &Q,
            std::vector<std::pair<osmium::Location, osmium::Location>> &bounding_boxes,
            std::vector<std::vector<osmium::object_id_type>> &closed_ways,
            std::unordered_map<osmium::object_id_type, osmium::Location> &node_locations)
{
    std::vector<long> matched_ways_bounding_boxes_indices;
    std::vector<std::pair<osmium::Location, osmium::Location>> crossed_bounding_boxes;
    std::vector<unsigned int> crossed_ways_bounding_boxes_indices;
    // Check if the casting ray crosses the bounding box
    for(std::size_t i = 0; i<bounding_boxes.size(); i++){
        if(isPointInBox(Q, bounding_boxes[i].first, bounding_boxes[i].second))
            crossed_ways_bounding_boxes_indices.push_back(i);
    }

    std::cout << "\nMatched bounding boxes: " << crossed_ways_bounding_boxes_indices.size() << std::endl;


    // Comment
    bool parity_odd = 0;
    for(std::size_t i = 0; i<closed_ways.size(); i++){
        if(isPointInPolygon(Q, closed_ways[i], node_locations))                                // if Q is in the closed way "land", then naturlich Land
            return false;
        // parity_odd ^= isPointInPolygon(Q, closed_ways[i], node_locations);                                // if Q is in the closed way "land", then naturlich Land

    }

    //Uncomment // but change to boxes bounding the test point
    // for(std::size_t i = 0; i<crossed_ways_bounding_boxes_indices.size(); i++){
    //     // std::vector<osmium::object_id_type> way = closed_ways[crossed_ways_bounding_boxes_indices[i]];
    //     if(isPointInPolygon(Q,
    //                         closed_ways[crossed_ways_bounding_boxes_indices[i]],
    //                         node_locations))                                // if Q is in the closed way "land", then naturlich Land
    //         return false;
    // }

    // std::cout << "\nNot inside any Land" << std::endl;

    return 1^parity_odd;
}

int main_task3(int argc, char* argv[])
{
    std::string input_file_name;

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " INPUT-FILE\n";
        // return 1;
        input_file_name = "antarctica-latest.osm.pbf";
    }
    else
        input_file_name = argv[1];

    std::vector<std::vector<osmium::object_id_type>> final_closed_ways;
    std::unordered_map<osmium::object_id_type, osmium::Location> node_locations;

    get_final_closed_ways(input_file_name, final_closed_ways, node_locations);

    std::cout << node_locations.size() << std::endl;
    std::cout << "test in bounding boxes" << std::endl;

    std::vector<std::pair<osmium::Location, osmium::Location>> bounding_boxes;
    getBoundingBox(final_closed_ways, node_locations, bounding_boxes);
    std::cout << "number of bounding boxes: " << bounding_boxes.size() << std::endl;

    std::vector<osmium::Location> test_points;
    // test_points.push_back(osmium::Location(-26.732548749488956, 10.671596281947856));
    // test_points.push_back(osmium::Location(11.939290077225365, 12.162589122776751));
    // test_points.push_back(osmium::Location(16.193990227322075, -80.31058318376975));
    test_points.push_back(osmium::Location(-23.30866463537828, -85.05112877980659)); // In Antarctica
    test_points.push_back(osmium::Location(122.32121620632046, -72.40337730584133)); // In Antarctica other side
    test_points.push_back(osmium::Location(61.6524702239368, 76.2930632732388)); // In Russia
    test_points.push_back(osmium::Location(-38.48528429954149, 77.52013957622279)); // In Greenland
    test_points.push_back(osmium::Location(-89.46894116718542, 85.05112877980659)); // In Water near north pole
    test_points.push_back(osmium::Location(-183.2858570662184, 81.88365705783642)); // In Water near north pole  // Doesn't work, throws an error: osmium::invalid_location
    test_points.push_back(osmium::Location(161.17794752875415, 5.768768463425289)); // In Water Pacific Ocean
    test_points.push_back(osmium::Location(-190.18369831686493, 82.69225385345266)); // In Water North pole   // error: osmium::invalid_location

    for(auto Q : test_points){
        std::cout << Q << std::endl;
        std::cout << (isWater(Q, bounding_boxes, final_closed_ways, node_locations)? "----Water----" : "----Land----") << std::endl;
    }
    for(auto location: crossed_nodes)
        std::cout << location << std::endl;
    // std::cout << isEdgeCrossed(osmium::Location(3.0,2.0), osmium::Location(2,3), osmium::Location(4,3)) << std::endl;

    return 0;
}

