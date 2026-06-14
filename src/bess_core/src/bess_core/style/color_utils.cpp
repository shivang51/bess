#include "bess_core/style/color_utils.h"
#include "fwd.hpp"

#include <numbers>

namespace Bess::Core::Style {
    namespace {
        // Got helpers from following source:
        // I have only copied what was required and changed its types to match
        // glm Vec3 -> glm::highp_dvec3
        // https://github.com/material-foundation/material-color-utilities/blob/main/cpp/utils/utils.cc

        using Vec3 = glm::highp_dvec3;
        typedef uint32_t Argb;

        int Delinearized(const double rgb_component) {
            double normalized = rgb_component / 100;
            double delinearized = 0;
            if (normalized <= 0.0031308) {
                delinearized = normalized * 12.92;
            } else {
                delinearized =
                    (1.055 * std::pow(normalized, 1.0 / 2.4)) - 0.055;
            }
            return std::clamp((int)round(delinearized * 255.0), 0, 255);
        }

        double Linearized(const int rgb_component) {
            double normalized = rgb_component / 255.0;
            if (normalized <= 0.040449936) {
                return normalized / 12.92 * 100.0;
            } else {
                return std::pow((normalized + 0.055) / 1.055, 2.4) * 100.0;
            }
        }

        double LstarFromY(double y) {
            static const double e = 216.0 / 24389.0;
            double yNormalized = y / 100.0;
            if (yNormalized <= e) {
                return (24389.0 / 27.0) * yNormalized;
            } else {
                return (116.0 * std::pow(yNormalized, 1.0 / 3.0)) - 16.0;
            }
        }

        double LstarFromArgb(Argb argb) {
            // xyz from argb
            int red = (int)((argb & 0x00ff0000) >> 16);
            int green = (int)((argb & 0x0000ff00) >> 8);
            int blue = (int)((argb & 0x000000ff));
            double red_l = Linearized(red);
            double green_l = Linearized(green);
            double blue_l = Linearized(blue);
            double y =
                (0.2126 * red_l) + (0.7152 * green_l) + (0.0722 * blue_l);
            return LstarFromY(y);
        }

        // The signum function.
        //
        // Returns 1 if num > 0, -1 if num < 0, and 0 if num = 0
        int Signum(double num) {
            if (num < 0) {
                return -1;
            } else if (num == 0) {
                return 0;
            } else {
                return 1;
            }
        }

        // From:
        // https://github.com/material-foundation/material-color-utilities/blob/main/cpp/cam/cam.h
        struct Cam {
            double hue = 0.0;
            double chroma = 0.0;
            double j = 0.0;
            double q = 0.0;
            double m = 0.0;
            double s = 0.0;

            double jstar = 0.0;
            double astar = 0.0;
            double bstar = 0.0;
        };

        struct ViewingConditions {
            double adapting_luminance = 0.0;
            double background_lstar = 0.0;
            double surround = 0.0;
            bool discounting_illuminant = false;
            double background_y_to_white_point_y = 0.0;
            double aw = 0.0;
            double nbb = 0.0;
            double ncb = 0.0;
            double c = 0.0;
            double n_c = 0.0;
            double fl = 0.0;
            double fl_root = 0.0;
            double z = 0.0;

            double white_point[3] = {0.0, 0.0, 0.0};
            double rgb_d[3] = {0.0, 0.0, 0.0};
        };

        constexpr double kPi = std::numbers::pi;
        // Sanitizes a degree measure as a floating-point number.
        //
        // Returns a degree measure between 0.0 (inclusive) and 360.0
        // (exclusive).
        double SanitizeDegreesDouble(const double degrees) {
            if (degrees < 0.0) {
                return fmod(degrees, 360.0) + 360;
            } else if (degrees >= 360.0) {
                return fmod(degrees, 360.0);
            } else {
                return degrees;
            }
        }

        Cam CamFromIntAndViewingConditions(
            Argb argb, const ViewingConditions &viewing_conditions) {
            // XYZ from ARGB, inlined.
            int red = (int)((argb & 0x00ff0000) >> 16);
            int green = (int)((argb & 0x0000ff00) >> 8);
            int blue = (int)((argb & 0x000000ff));
            double red_l = Linearized(red);
            double green_l = Linearized(green);
            double blue_l = Linearized(blue);
            double x = (0.41233895 * red_l) + (0.35762064 * green_l) +
                       (0.18051042 * blue_l);
            double y =
                (0.2126 * red_l) + (0.7152 * green_l) + (0.0722 * blue_l);
            double z = (0.01932141 * red_l) + (0.11916382 * green_l) +
                       (0.95034478 * blue_l);

            // Convert XYZ to 'cone'/'rgb' responses
            double r_c = (0.401288 * x) + (0.650173 * y) - (0.051461 * z);
            double g_c = (-0.250268 * x) + (1.204414 * y) + (0.045854 * z);
            double b_c = (-0.002079 * x) + (0.048952 * y) + (0.953127 * z);

            // Discount illuminant.
            double r_d = viewing_conditions.rgb_d[0] * r_c;
            double g_d = viewing_conditions.rgb_d[1] * g_c;
            double b_d = viewing_conditions.rgb_d[2] * b_c;

            // Chromatic adaptation.
            double r_af = pow(viewing_conditions.fl * fabs(r_d) / 100.0, 0.42);
            double g_af = pow(viewing_conditions.fl * fabs(g_d) / 100.0, 0.42);
            double b_af = pow(viewing_conditions.fl * fabs(b_d) / 100.0, 0.42);
            double r_a = Signum(r_d) * 400.0 * r_af / (r_af + 27.13);
            double g_a = Signum(g_d) * 400.0 * g_af / (g_af + 27.13);
            double b_a = Signum(b_d) * 400.0 * b_af / (b_af + 27.13);

            // Redness-greenness
            double a = ((11.0 * r_a) + (-12.0 * g_a) + b_a) / 11.0;
            double b = (r_a + g_a - (2.0 * b_a)) / 9.0;
            double u = ((20.0 * r_a) + (20.0 * g_a) + (21.0 * b_a)) / 20.0;
            double p2 = ((40.0 * r_a) + (20.0 * g_a) + b_a) / 20.0;

            double radians = atan2(b, a);
            double degrees = radians * 180.0 / kPi;
            double hue = SanitizeDegreesDouble(degrees);
            double hue_radians = hue * kPi / 180.0;
            double ac = p2 * viewing_conditions.nbb;

            double j = 100.0 * pow(ac / viewing_conditions.aw,
                                   viewing_conditions.c * viewing_conditions.z);
            double q = (4.0 / viewing_conditions.c) * sqrt(j / 100.0) *
                       (viewing_conditions.aw + 4.0) *
                       viewing_conditions.fl_root;
            double hue_prime = hue < 20.14 ? hue + 360 : hue;
            double e_hue = 0.25 * (cos((hue_prime * kPi / 180.0) + 2.0) + 3.8);
            double p1 = 50000.0 / 13.0 * e_hue * viewing_conditions.n_c *
                        viewing_conditions.ncb;
            double t = p1 * sqrt((a * a) + (b * b)) / (u + 0.305);
            double alpha =
                pow(t, 0.9) *
                pow(1.64 -
                        pow(0.29,
                            viewing_conditions.background_y_to_white_point_y),
                    0.73);
            double c = alpha * sqrt(j / 100.0);
            double m = c * viewing_conditions.fl_root;
            double s = 50.0 * sqrt((alpha * viewing_conditions.c) /
                                   (viewing_conditions.aw + 4.0));
            double jstar = (1.0 + (100.0 * 0.007)) * j / (1.0 + (0.007 * j));
            double mstar = 1.0 / 0.0228 * log(1.0 + (0.0228 * m));
            double astar = mstar * cos(hue_radians);
            double bstar = mstar * sin(hue_radians);
            return {hue, c, j, q, m, s, jstar, astar, bstar};
        }

        // From
        // https://github.com/material-foundation/material-color-utilities/blob/main/cpp/cam/viewing_conditions.h#L49
        constexpr ViewingConditions kDefaultViewingConditions = {
            11.725676537,
            50.000000000,
            2.000000000,
            false,
            0.184186503,
            29.981000900,
            1.016919255,
            1.016919255,
            0.689999998,
            1.000000000,
            0.388481468,
            0.789482653,
            1.909169555,
            {95.047, 100.0, 108.883},
            {1.021177769, 0.986307740, 0.933960497},
        };

        constexpr glm::highp_dmat3x3 kLinrgbFromScaledDiscount = {
            {
                1373.2198709594231,
                -1100.4251190754821,
                -7.278681089101213,
            },
            {
                -271.815969077903,
                559.6580465940733,
                -32.46047482791194,
            },
            {
                1.9622899599665666,
                -57.173814538844006,
                308.7233197812385,
            },
        };

        Cam CamFromInt(Argb argb) {
            return CamFromIntAndViewingConditions(argb,
                                                  kDefaultViewingConditions);
        }

        double YFromLstar(double lstar) {
            static const double ke = 8.0;
            if (lstar > ke) {
                double cube_root = (lstar + 16.0) / 116.0;
                double cube = cube_root * cube_root * cube_root;
                return cube * 100.0;
            } else {
                return lstar / (24389.0 / 27.0) * 100.0;
            }
        }

        Argb ArgbFromRgb(const int red, const int green, const int blue) {
            return 0xFF000000 | ((red & 0xff) << 16) | ((green & 0xff) << 8) |
                   (blue & 0xff);
        }

        Argb IntFromLstar(const double lstar) {
            double y = YFromLstar(lstar);
            int component = Delinearized(y);
            return ArgbFromRgb(component, component, component);
        }

        double InverseChromaticAdaptation(double adapted) {
            double adapted_abs = std::abs(adapted);
            double base = fmax(0, 27.13 * adapted_abs / (400.0 - adapted_abs));
            return Signum(adapted) * pow(base, 1.0 / 0.42);
        }

        // Converts a color from linear RGB components to ARGB format.
        Argb ArgbFromLinrgb(Vec3 linrgb) {
            int r = Delinearized(linrgb.x);
            int g = Delinearized(linrgb.y);
            int b = Delinearized(linrgb.z);

            return 0xFF000000 | ((r & 0x0ff) << 16) | ((g & 0x0ff) << 8) |
                   (b & 0x0ff);
        }

        constexpr double kYFromLinrgb[3] = {0.2126, 0.7152, 0.0722};

        /**
         * Finds a color with the given hue, chroma, and Y.
         *
         * @param hue_radians The desired hue in radians.
         * @param chroma The desired chroma.
         * @param y The desired Y.
         * @return The desired color as a hexadecimal integer, if found; 0
         * otherwise.
         */
        Argb FindResultByJ(double hue_radians, double chroma, double y) {
            // Initial estimate of j.
            double j = sqrt(y) * 11.0;
            // ===========================================================
            // Operations inlined from Cam16 to avoid repeated calculation
            // ===========================================================
            ViewingConditions viewing_conditions = kDefaultViewingConditions;
            double t_inner_coeff =
                1 /
                pow(1.64 -
                        pow(0.29,
                            viewing_conditions.background_y_to_white_point_y),
                    0.73);
            double e_hue = 0.25 * (cos(hue_radians + 2.0) + 3.8);
            double p1 = e_hue * (50000.0 / 13.0) * viewing_conditions.n_c *
                        viewing_conditions.ncb;
            double h_sin = sin(hue_radians);
            double h_cos = cos(hue_radians);
            for (int iteration_round = 0; iteration_round < 5;
                 iteration_round++) {
                // ===========================================================
                // Operations inlined from Cam16 to avoid repeated calculation
                // ===========================================================
                double j_normalized = j / 100.0;
                double alpha = chroma == 0.0 || j == 0.0
                                   ? 0.0
                                   : chroma / sqrt(j_normalized);
                double t = pow(alpha * t_inner_coeff, 1.0 / 0.9);
                double ac =
                    viewing_conditions.aw *
                    pow(j_normalized,
                        1.0 / viewing_conditions.c / viewing_conditions.z);
                double p2 = ac / viewing_conditions.nbb;
                double gamma =
                    23.0 * (p2 + 0.305) * t /
                    ((23.0 * p1) + (11 * t * h_cos) + (108.0 * t * h_sin));
                double a = gamma * h_cos;
                double b = gamma * h_sin;
                double r_a =
                    ((460.0 * p2) + (451.0 * a) + (288.0 * b)) / 1403.0;
                double g_a =
                    ((460.0 * p2) - (891.0 * a) - (261.0 * b)) / 1403.0;
                double b_a =
                    ((460.0 * p2) - (220.0 * a) - (6300.0 * b)) / 1403.0;
                double r_c_scaled = InverseChromaticAdaptation(r_a);
                double g_c_scaled = InverseChromaticAdaptation(g_a);
                double b_c_scaled = InverseChromaticAdaptation(b_a);
                Vec3 scaled = {r_c_scaled, g_c_scaled, b_c_scaled};
                Vec3 linrgb = scaled * kLinrgbFromScaledDiscount;
                // ===========================================================
                // Operations inlined from Cam16 to avoid repeated calculation
                // ===========================================================
                if (linrgb.x < 0 || linrgb.y < 0 || linrgb.z < 0) {
                    return 0;
                }
                double k_r = kYFromLinrgb[0];
                double k_g = kYFromLinrgb[1];
                double k_b = kYFromLinrgb[2];
                double fnj =
                    (k_r * linrgb.x) + (k_g * linrgb.y) + (k_b * linrgb.z);
                if (fnj <= 0) {
                    return 0;
                }
                if (iteration_round == 4 || std::abs(fnj - y) < 0.002) {
                    if (linrgb.x > 100.01 || linrgb.y > 100.01 ||
                        linrgb.z > 100.01) {
                        return 0;
                    }
                    return ArgbFromLinrgb(linrgb);
                }
                // Iterates with Newton method,
                // Using 2 * fn(j) / j as the approximation of fn'(j)
                j = j - ((fnj - y) * j / (2 * fnj));
            }
            return 0;
        }

        int CriticalPlaneBelow(double x) {
            return (int)floor(x - 0.5);
        }

        int CriticalPlaneAbove(double x) {
            return (int)ceil(x - 0.5);
        }

        /**
         * Sanitizes a small enough angle in radians.
         *
         * @param angle An angle in radians; must not deviate too much from 0.
         * @return A coterminal angle between 0 and 2pi.
         */
        double SanitizeRadians(double angle) {
            return fmod(angle + (kPi * 8), kPi * 2);
        }

        bool AreInCyclicOrder(double a, double b, double c) {
            double delta_a_b = SanitizeRadians(b - a);
            double delta_a_c = SanitizeRadians(c - a);
            return delta_a_b < delta_a_c;
        }

        double ChromaticAdaptation(double component) {
            double af = pow(std::abs(component), 0.42);
            return Signum(component) * 400.0 * af / (af + 27.13);
        }

        constexpr glm::highp_dmat3x3 kScaledDiscountFromLinrgb = {
            {
                0.001200833568784504,
                0.002389694492170889,
                0.0002795742885861124,
            },
            {
                0.0005891086651375999,
                0.0029785502573438758,
                0.0003270666104008398,
            },
            {
                0.00010146692491640572,
                0.0005364214359186694,
                0.0032979401770712076,
            },
        };

        constexpr double kCriticalPlanes[255] = {
            0.015176349177441876, 0.045529047532325624, 0.07588174588720938,
            0.10623444424209313,  0.13658714259697685,  0.16693984095186062,
            0.19729253930674434,  0.2276452376616281,   0.2579979360165119,
            0.28835063437139563,  std::numbers::inv_pi, 0.350925934958123,
            0.3848314933096426,   0.42057480301049466,  0.458183274052838,
            0.4976837250274023,   0.5391024159806381,   0.5824650784040898,
            0.6277969426914107,   0.6751227633498623,   0.7244668422128921,
            0.775853049866786,    0.829304845476233,    0.8848452951698498,
            0.942497089126609,    1.0022825574869039,   1.0642236851973577,
            1.1283421258858297,   1.1946592148522128,   1.2631959812511864,
            1.3339731595349034,   1.407011200216447,    1.4823302800086415,
            1.5599503113873272,   1.6398909516233677,   1.7221716113234105,
            1.8068114625156377,   1.8938294463134073,   1.9832442801866852,
            2.075074464868551,    2.1693382909216234,   2.2660538449872063,
            2.36523901573795,     2.4669114995532007,   2.5710888059345764,
            2.6777882626779785,   2.7870270208169257,   2.898822059350997,
            3.0131901897720907,   3.1301480604002863,   3.2497121605402226,
            3.3718988244681087,   3.4967242352587946,   3.624204428461639,
            3.754355295633311,    3.887192587735158,    4.022731918402185,
            4.160988767090289,    4.301978482107941,    4.445716283538092,
            4.592217266055746,    4.741496401646282,    4.893568542229298,
            5.048448422192488,    5.20615066083972,     5.3666897647573375,
            5.5300801301023865,   5.696336044816294,    5.865471690767354,
            6.037501145825082,    6.212438385869475,    6.390297286737924,
            6.571091626112461,    6.7548350853498045,   6.941541251256611,
            7.131223617812143,    7.323895587840543,    7.5195704746346665,
            7.7182615035334345,   7.919981813454504,    8.124744458384042,
            8.332562408825165,    8.543448553206703,    8.757415699253682,
            8.974476575321063,    9.194643831691977,    9.417930041841839,
            9.644347703669503,    9.873909240696694,    10.106627003236781,
            10.342513269534024,   10.58158024687427,    10.8238400726681,
            11.069304815507364,   11.317986476196008,   11.569896988756009,
            11.825048221409341,   12.083451977536606,   12.345119996613247,
            12.610063955123938,   12.878295467455942,   13.149826086772048,
            13.42466730586372,    13.702830557985108,   13.984327217668513,
            14.269168601521828,   14.55736596900856,    14.848930523210871,
            15.143873411576273,   15.44220572664832,    15.743938506781891,
            16.04908273684337,    16.35764934889634,    16.66964922287304,
            16.985093187232053,   17.30399201960269,    17.62635644741625,
            17.95219714852476,    18.281524751807332,   18.614349837764564,
            18.95068293910138,    19.290534541298456,   19.633915083172692,
            19.98083495742689,    20.331304511189067,   20.685334046541502,
            21.042933821039977,   21.404114048223256,   21.76888489811322,
            22.137256497705877,   22.50923893145328,    22.884842241736916,
            23.264076429332462,   23.6469514538663,     24.033477234264016,
            24.42366364919083,    24.817520537484558,   25.21505769858089,
            25.61628489293138,    26.021211842414342,   26.429848230738664,
            26.842203703840827,   27.258287870275353,   27.678110301598522,
            28.10168053274597,    28.529008062403893,   28.96010235337422,
            29.39497283293396,    29.83362889318845,    30.276079891419332,
            30.722335150426627,   31.172403958865512,   31.62629557157785,
            32.08401920991837,    32.54558406207592,    33.010999283389665,
            33.4802739966603,     33.953417292456834,   34.430438229418264,
            34.911345834551085,   35.39614910352207,    35.88485700094671,
            36.37747846067349,    36.87402238606382,    37.37449765026789,
            37.87891309649659,    38.38727753828926,    38.89959975977785,
            39.41588851594697,    39.93615253289054,    40.460400508064545,
            40.98864111053629,    41.520882981230194,   42.05713473317016,
            42.597404951718396,   43.141702194811224,   43.6900349931913,
            44.24241185063697,    44.798841244188324,   45.35933162437017,
            45.92389141541209,    46.49252901546552,    47.065252796817916,
            47.64207110610409,    48.22299226451468,    48.808024568002054,
            49.3971762874833,     49.9904556690408,     50.587870934119984,
            51.189430279724725,   51.79514187861014,    52.40501387947288,
            53.0190544071392,     53.637271562750364,   54.259673423945976,
            54.88626804504493,    55.517063457223934,   56.15206766869424,
            56.79128866487574,    57.43473440856916,    58.08241284012621,
            58.734331877617365,   59.39049941699807,    60.05092333227251,
            60.715611475655585,   61.38457167773311,    62.057811747619894,
            62.7353394731159,     63.417162620860914,   64.10328893648692,
            64.79372614476921,    65.48848194977529,    66.18756403501224,
            66.89098006357258,    67.59873767827808,    68.31084450182222,
            69.02730813691093,    69.74813616640164,    70.47333615344107,
            71.20291564160104,    71.93688215501312,    72.67524319850172,
            73.41800625771542,    74.16517879925733,    74.9167682708136,
            75.67278210128072,    76.43322770089146,    77.1981124613393,
            77.96744375590167,    78.74122893956174,    79.51947534912904,
            80.30219030335869,    81.08938110306934,    81.88105503125999,
            82.67721935322541,    83.4778813166706,     84.28304815182372,
            85.09272707154808,    85.90692527145302,    86.72564993000343,
            87.54890820862819,    88.3767072518277,     89.2090541872801,
            90.04595612594655,    90.88742016217518,    91.73345337380438,
            92.58406282226491,    93.43925555268066,    94.29903859396902,
            95.16341895893969,    96.03240364439274,    96.9059996312159,
            97.78421388448044,    98.6670533535366,     99.55452497210776,
        };

        /**
         * Returns the hue of a linear RGB color in CAM16.
         *
         * @param linrgb The linear RGB coordinates of a color.
         * @return The hue of the color in CAM16, in radians.
         */
        double HueOf(Vec3 linrgb) {
            Vec3 scaledDiscount = linrgb * kScaledDiscountFromLinrgb;
            double r_a = ChromaticAdaptation(scaledDiscount.x);
            double g_a = ChromaticAdaptation(scaledDiscount.y);
            double b_a = ChromaticAdaptation(scaledDiscount.z);
            // redness-greenness
            double a = ((11.0 * r_a) + (-12.0 * g_a) + b_a) / 11.0;
            // yellowness-blueness
            double b = (r_a + g_a - (2.0 * b_a)) / 9.0;
            return atan2(b, a);
        }

        bool IsBounded(double x) {
            return 0.0 <= x && x <= 100.0;
        }

        /**
         * Returns the nth possible vertex of the polygonal intersection.
         *
         * @param y The Y value of the plane.
         * @param n The zero-based index of the point. 0 <= n <= 11.
         * @return The nth possible vertex of the polygonal intersection of the
         * y plane and the RGB cube, in linear RGB coordinates, if it exists. If
         * this possible vertex lies outside of the cube,
         *     [-1.0, -1.0, -1.0] is returned.
         */
        Vec3 NthVertex(double y, int n) {
            double k_r = kYFromLinrgb[0];
            double k_g = kYFromLinrgb[1];
            double k_b = kYFromLinrgb[2];
            double coord_a = n % 4 <= 1 ? 0.0 : 100.0;
            double coord_b = n % 2 == 0 ? 0.0 : 100.0;
            if (n < 4) {
                double g = coord_a;
                double b = coord_b;
                double r = (y - (g * k_g) - (b * k_b)) / k_r;
                if (IsBounded(r)) {
                    return (Vec3){r, g, b};
                } else {
                    return (Vec3){-1.0, -1.0, -1.0};
                }
            } else if (n < 8) {
                double b = coord_a;
                double r = coord_b;
                double g = (y - (r * k_r) - (b * k_b)) / k_g;
                if (IsBounded(g)) {
                    return (Vec3){r, g, b};
                } else {
                    return (Vec3){-1.0, -1.0, -1.0};
                }
            } else {
                double r = coord_a;
                double g = coord_b;
                double b = (y - (r * k_r) - (g * k_g)) / k_b;
                if (IsBounded(b)) {
                    return (Vec3){r, g, b};
                } else {
                    return (Vec3){-1.0, -1.0, -1.0};
                }
            }
        }

        /**
         * Finds the segment containing the desired color.
         *
         * @param y The Y value of the color.
         * @param target_hue The hue of the color.
         * @return A list of two sets of linear RGB coordinates, each
         * corresponding to an endpoint of the segment containing the desired
         * color.
         */
        void BisectToSegment(double y, double target_hue, Vec3 out[2]) {
            Vec3 left = (Vec3){-1.0, -1.0, -1.0};
            Vec3 right = left;
            double left_hue = 0.0;
            double right_hue = 0.0;
            bool initialized = false;
            bool uncut = true;
            for (int n = 0; n < 12; n++) {
                Vec3 mid = NthVertex(y, n);
                if (mid.x < 0) {
                    continue;
                }
                double mid_hue = HueOf(mid);
                if (!initialized) {
                    left = mid;
                    right = mid;
                    left_hue = mid_hue;
                    right_hue = mid_hue;
                    initialized = true;
                    continue;
                }
                if (uncut || AreInCyclicOrder(left_hue, mid_hue, right_hue)) {
                    uncut = false;
                    if (AreInCyclicOrder(left_hue, target_hue, mid_hue)) {
                        right = mid;
                        right_hue = mid_hue;
                    } else {
                        left = mid;
                        left_hue = mid_hue;
                    }
                }
            }
            out[0] = left;
            out[1] = right;
        }

        Vec3 Midpoint(Vec3 a, Vec3 b) {
            return (Vec3){
                (a.x + b.x) / 2,
                (a.y + b.y) / 2,
                (a.z + b.z) / 2,
            };
        }

        double GetAxis(Vec3 vector, int axis) {
            switch (axis) {
            case 0:
                return vector.r;
            case 1:
                return vector.g;
            case 2:
                return vector.b;
            default:
                return -1.0;
            }
        }

        /**
         * Delinearizes an RGB component, returning a floating-point number.
         *
         * @param rgb_component 0.0 <= rgb_component <= 100.0, represents linear
         * R/G/B channel
         * @return 0.0 <= output <= 255.0, color channel converted to regular
         * RGB space
         */
        double TrueDelinearized(double rgb_component) {
            double normalized = rgb_component / 100.0;
            double delinearized = 0.0;
            if (normalized <= 0.0031308) {
                delinearized = normalized * 12.92;
            } else {
                delinearized = (1.055 * pow(normalized, 1.0 / 2.4)) - 0.055;
            }
            return delinearized * 255.0;
        }

        /**
         * Solves the lerp equation.
         *
         * @param source The starting number.
         * @param mid The number in the middle.
         * @param target The ending number.
         * @return A number t such that lerp(source, target, t) = mid.
         */
        double Intercept(double source, double mid, double target) {
            return (mid - source) / (target - source);
        }

        Vec3 LerpPoint(Vec3 source, double t, Vec3 target) {
            return (Vec3){
                source.x + ((target.x - source.x) * t),
                source.y + ((target.y - source.y) * t),
                source.z + ((target.z - source.z) * t),
            };
        }

        /**
         * Intersects a segment with a plane.
         *
         * @param source The coordinates of point A.
         * @param coordinate The R-, G-, or B-coordinate of the plane.
         * @param target The coordinates of point B.
         * @param axis The axis the plane is perpendicular with. (0: R, 1: G, 2:
         * B)
         * @return The intersection point of the segment AB with the plane
         * R=coordinate, G=coordinate, or B=coordinate
         */
        Vec3
        SetCoordinate(Vec3 source, double coordinate, Vec3 target, int axis) {
            double t = Intercept(
                GetAxis(source, axis), coordinate, GetAxis(target, axis));
            return LerpPoint(source, t, target);
        }

        /**
         * Finds a color with the given Y and hue on the boundary of the cube.
         *
         * @param y The Y value of the color.
         * @param target_hue The hue of the color.
         * @return The desired color, in linear RGB coordinates.
         */
        Vec3 BisectToLimit(double y, double target_hue) {
            Vec3 segment[2];
            BisectToSegment(y, target_hue, segment);
            Vec3 left = segment[0];
            double left_hue = HueOf(left);
            Vec3 right = segment[1];
            for (int axis = 0; axis < 3; axis++) {
                if (GetAxis(left, axis) != GetAxis(right, axis)) {
                    int l_plane = -1;
                    int r_plane = 255;
                    if (GetAxis(left, axis) < GetAxis(right, axis)) {
                        l_plane = CriticalPlaneBelow(
                            TrueDelinearized(GetAxis(left, axis)));
                        r_plane = CriticalPlaneAbove(
                            TrueDelinearized(GetAxis(right, axis)));
                    } else {
                        l_plane = CriticalPlaneAbove(
                            TrueDelinearized(GetAxis(left, axis)));
                        r_plane = CriticalPlaneBelow(
                            TrueDelinearized(GetAxis(right, axis)));
                    }
                    for (int i = 0; i < 8; i++) {
                        if (abs(r_plane - l_plane) <= 1) {
                            break;
                        } else {
                            int m_plane = (int)floor((l_plane + r_plane) / 2.0);
                            double mid_plane_coordinate =
                                kCriticalPlanes[m_plane];
                            Vec3 mid = SetCoordinate(
                                left, mid_plane_coordinate, right, axis);
                            double mid_hue = HueOf(mid);
                            if (AreInCyclicOrder(
                                    left_hue, target_hue, mid_hue)) {
                                right = mid;
                                r_plane = m_plane;
                            } else {
                                left = mid;
                                left_hue = mid_hue;
                                l_plane = m_plane;
                            }
                        }
                    }
                }
            }
            return Midpoint(left, right);
        }

        Argb SolveToInt(double hue_degrees, double chroma, double lstar) {
            if (chroma < 0.0001 || lstar < 0.0001 || lstar > 99.9999) {
                return IntFromLstar(lstar);
            }
            hue_degrees = SanitizeDegreesDouble(hue_degrees);
            double hue_radians = hue_degrees / 180 * kPi;
            double y = YFromLstar(lstar);
            Argb exact_answer = FindResultByJ(hue_radians, chroma, y);
            if (exact_answer != 0) {
                return exact_answer;
            }
            Vec3 linrgb = BisectToLimit(y, hue_radians);
            return ArgbFromLinrgb(linrgb);
        }

    } // namespace

    constexpr HctColor HctColor::fromColor(const Renderer::Color &color) {
        Argb argb = color.toARGB8();
        Cam cam = CamFromInt(argb);

        HctColor hct;
        hct.hue = cam.hue;
        hct.chroma = cam.chroma;
        hct.tone = LstarFromArgb(argb);
        return hct;
    }

    constexpr Renderer::Color HctColor::toColor() const {
        return Renderer::Color::FromARGB(SolveToInt(hue, chroma, tone));
    }
} // namespace Bess::Core::Style
