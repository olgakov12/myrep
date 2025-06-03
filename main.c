#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lodepng.h"
/*здравствуйте!*/
/*изначально левая часть изображения показалась мне менее яркой и чёткой, поэтому я решила обрабатывать левую и правую части по-разному, поэтому в функциях часто будет "ширина" / 2 и тд, я отдельно прокомментирую*/

/*часть 1, функции для считывания и вывода изображения, перевод в чб*/
unsigned char* load_image(const char* filename, unsigned* width, unsigned* height){
    unsigned char* image = NULL;
    unsigned error = lodepng_decode32_file(&image, width, height, filename);
    
    if(error){
        printf("Ошибка загрузки: %s\n", lodepng_error_text(error));
        return NULL;
    }
    return image;
}


void write_png(const char* filename, const unsigned char* image, unsigned width, unsigned height){
    unsigned char* png;
    unsigned long pngsize;
    unsigned error = lodepng_encode32(&png, &pngsize, image, width, height);
    
    if(error == 0) lodepng_save_file(png, pngsize, filename);
    else printf("Ошибка сохранения: %s\n", lodepng_error_text(error));
    
    free(png);
}


unsigned char* gray_to_rgba(const unsigned char* gray, unsigned width, unsigned height) {
    unsigned char* rgba = malloc(width * height * 4);
    if(!rgba) return NULL;

    for(unsigned i = 0; i < width * height; i++){
        rgba[i * 4] = gray[i];
        rgba[i * 4 + 1] = gray[i];
        rgba[i * 4 + 2] = gray[i];
        rgba[i * 4 + 3] = 255;
    }
    return rgba;
}


unsigned char* convert_to_grayscale(const unsigned char* rgba_image, unsigned width, unsigned height) {
    unsigned char* gray_image = malloc(width * height);
    if(!gray_image) return NULL;
    
    for(unsigned i = 0; i < width * height; i++) {
        unsigned char r = rgba_image[i * 4];
        unsigned char g = rgba_image[i * 4 + 1];
        unsigned char b = rgba_image[i * 4 + 2];
        
        gray_image[i] = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
    }
    return gray_image;
}


/*часть 2, функции для работы с изображением*/
/*функция для работы с контрастностью изображения*/
void contrast_stretch(unsigned char* gray, unsigned width, unsigned height){
    /*первый цикл обрабатывает только левую половину*/
    for(unsigned y = 0; y < height; y++){
            for(unsigned x = 0; x < width / 2; x++){
                unsigned ind = y * width + x;
                gray[ind] = (unsigned char)(200.0 * (gray[ind] - 50) / (400 - 120));
            }
        }
    /*далее правую часть изображения я разбила ещё на две части - верхнюю и нижнюю*/
    /*цикл обрабатывает нижнюю правую часть*/
    for(unsigned y = height / 2; y < height; y++){
            for (unsigned x = width / 2; x < width; x++){
                unsigned ind = y * width + x;
                gray[ind] = (unsigned char)(160.0 * (gray[ind] - 70) / (230 - 120));
            }
        }
    /*цикл обрабатывает верхнюю правую часть*/
    for(unsigned y = 0; y < height / 2; y++){
            for(unsigned x = width / 2; x < width; x++){
                unsigned ind = y * width + x;
                gray[ind] = (unsigned char)(160.0 * (gray[ind] - 30) / (250 - 120));
            }
        }
}



int compare(const void* a, const void* b){
    return (*(unsigned char*)a - *(unsigned char*)b);
}

/*медианный фильтр. как мне кажется, самая лучшая вещь!*/
void median_filter(const unsigned char* input, unsigned char* output, unsigned width, unsigned height){
    /*тоже левую отдельно, правую отдельно*/
    /*поскольку медианный фильтр обрабатывает изображение с помощью "окон", по правой части пройдёмся окном 3*3 со смещением на 1 пиксель от центра, а по левовй окном 7*7 со смещением на 3 пикселей от центра*/
    int edge3 = 1;/*радиус окна для правой части*/
    int edge5 = 3;/*радиус окна для левой части*/
    
    /*для правой части - окно с меньшим радиусом, чтобы сохранить более чёткие границы, а в левой, наоборот, большим*/
    for(unsigned y = 0; y < height; y++){
        for(unsigned x = 0; x < width; x++){
            int window_size, edge;
            if(x < width / 2){
                window_size = 7;
                edge = edge5;
            }
            else{
                window_size = 2;
                edge = edge3;
            }

            int window_area = window_size * window_size;
            unsigned char* window = malloc(window_area);
            int count = 0;/*счётчик собранных пикселей*/

            /*цикл для пикселей в самом окне*/
            for(int dy = -edge; dy <= edge; dy++){
                for(int dx = -edge; dx <= edge; dx++){
                    int nx = x + dx;
                    int ny = y + dy;
                    /*проверяем границы изображения*/
                    if(nx >= 0 && nx < (int)width && ny >= 0 && ny < (int)height)  window[count++] = input[ny * width + nx];
                }
            }
            /*сортируем пиксели для поиска медианы*/
            qsort(window, count, sizeof(unsigned char), compare);
            
            output[y * width + x] = window[count / 2];/*медиана*/
            
            free(window);
        }
    }
}

/*фильтр Собеля. исключительно для левой половины*/
unsigned char* sobel_filter(const unsigned char* gray, unsigned width, unsigned height){
    unsigned char* output = malloc(width * height);
    if(!output) return NULL;
    
    double Gx[3][3] = {
        {-0.5, -0.4, 1},
        {-0.9, 0, 1},
        {-0.9, -0.4, 1}
    };
    
    double Gy[3][3] = {
        {0.9, 1,  1},
        { 0,  -0.4,  1},
        {-0.3, -0.3, -0.3}
    };
    
    for(unsigned i = 0; i < width * height; i++)
        output[i] = gray[i];
    
    /*фильтр Собеля без границ*/
    for(unsigned y = 1; y < height - 1; y++){
        for(unsigned x = 1; x < (width / 2) - 1; x++){
            double sumX = 0.0, sumY = 0.0;
            
            for(int ky = -1; ky <= 1; ky++){
                for(int kx = -1; kx <= 1; kx++){
                    int pixel = gray[(y + ky) * width + (x + kx)];
                    sumX += Gx[ky + 1][kx + 1] * pixel;
                    sumY += Gy[ky + 1][kx + 1] * pixel;
                }
            }
            
            int magnitude = (int)(sqrt(sumX * sumX + sumY * sumY));
            if(magnitude > 255) magnitude = 255;
            if(magnitude < 0) magnitude = 0;
            
            output[y * width + x] = (unsigned char)magnitude;
        }
    }
    /*этот цикл только чтобы убрать белую полосу снизу у исходного изображения*/
    for(unsigned x = 0; x < width; x++){
        output[(height - 1) * width + x] = 0;
        output[(height - 2) * width + x] = 0;
    }
    
    return output;
}

/*фильтр Гаусса для всего изображения*/
unsigned char* gaussian_filter(unsigned char* gray, unsigned width, unsigned height){
    unsigned char* output = malloc(width * height);
    if(!output) return NULL;

    // Ядро Гаусса 3x3
    int kernel[3][3] = {
        {3, 10, 3},
        {10, 20, 10},
        {3, 10, 3}
    };
    int kernel_sum = 72;
    
    for(unsigned i = 0; i < width * height; i++)
        output[i] = gray[i];
    
    for(unsigned y = 0; y < height; y++){
        for(unsigned x = 0; x < width; x++){
            int sum = 0;
            int weight_sum = 0;

            /*применяем ядро*/
            for(int ky = -1; ky <= 1; ky++){
                for(int kx = -1; kx <= 1; kx++){
                    int nx = x + kx;
                    int ny = y + ky;

                    /*проверяем границы*/
                    if(nx >= 0 && nx < (int)width && ny >= 0 && ny < (int)height){
                        int weight = kernel[ky + 1][kx + 1];
                        sum += gray[ny * width + nx] * weight;
                        weight_sum += weight;
                    }
                }
            }

            output[y * width + x] = (unsigned char)(sum / weight_sum);
        }
    }

    return output;
}

/*часть 3, построение графа и выделение компонент связности*/
/*идея: будем хранить компоненты связности с помощью disjoint set union(эвристика ромашка), а объединять с помощью алгоритма Краскала для построения MST*/
typedef struct edge{
    int from;
    int to;
    int weight;
}Edge;


typedef struct uni{
    int* parent;
    int* size;
    int count;
}UnionFind;


UnionFind* uf_create(int count) {
    UnionFind* uf = (UnionFind*)malloc(sizeof(UnionFind));
    if(!uf) return NULL;
        
    uf->parent = (int*)malloc(count * sizeof(int));
    uf->size = (int*)malloc(count * sizeof(int));
    uf->count = count;

    for(int i = 0; i < count; i++){
        uf->parent[i] = i;/*на этапе создания каждый пиксель представляет из себя множество из одного элемента, в котором он и есть представитель*/
        uf->size[i] = 1;
    }
    return uf;
}


int uf_find(UnionFind* uf, int x){
    if(uf->parent[x] != x){
        uf->parent[x] = uf_find(uf, uf->parent[x]);
    }
    return uf->parent[x];
}


void uf_union(UnionFind* uf, int x, int y) {
    int rootX = uf_find(uf, x);
    int rootY = uf_find(uf, y);

    if(rootX != rootY){
        if(uf->size[rootX] < uf->size[rootY]){
            uf->parent[rootX] = rootY;
            uf->size[rootY] += uf->size[rootX];
        }
        else{
            uf->parent[rootY] = rootX;
            uf->size[rootX] += uf->size[rootY];
        }
    }
}

/*функция для добавления ребра с вычислением веса(разницы яркости)*/
void add_edge(unsigned char* gray, Edge* edges, int* edge_count, int from, int to) {
    int diff = abs(gray[from] - gray[to]);
    
    edges[*edge_count].from = from;
    edges[*edge_count].to = to;
    edges[*edge_count].weight = diff;
    (*edge_count)++;
}


int compare_edges(const void* a, const void* b) {
    Edge* ea = (Edge*)a;
    Edge* eb = (Edge*)b;
    return ea->weight - eb->weight;
}

/*сегментация изображения*/
void kruskal(unsigned char* gray, int width, int height, float threshold, UnionFind* uf) {
    int n = width * height;
    int max_edges = n * 8;/*для каждого пикселя рассматриваем 8 соседних пикселей*/
    Edge* edges = malloc(max_edges * sizeof(Edge));

    if(!edges){
        fprintf(stderr, "Ошибка выделения памяти для рёбер\n");
        return;
    }

    int edge_count = 0;
    for(int y = 0; y < height; y++){
        for(int x = 0; x < width; x++){
            int idx = y * width + x;
            /*правый сосед*/
            if(x + 1 < width) add_edge(gray, edges, &edge_count, idx, y * width + (x + 1));
            /*левый*/
            if(x - 1 >= 0) add_edge(gray, edges, &edge_count, idx, y * width + (x - 1));
            /*нижний*/
            if(y + 1 < height) add_edge(gray, edges, &edge_count, idx, (y + 1) * width + x);
            /*верний*/
            if(y - 1 >= 0) add_edge(gray, edges, &edge_count, idx, (y - 1) * width + x);
            /*нижний-правый*/
            if(x + 1 < width && y + 1 < height) add_edge(gray, edges, &edge_count, idx, (y + 1) * width + (x + 1));
            /*нижний-левый*/
            if(x - 1 >= 0 && y + 1 < height) add_edge(gray, edges, &edge_count, idx, (y + 1) * width + (x - 1));
            /*верхний-правый*/
            if(x + 1 < width && y - 1 >= 0) add_edge(gray, edges, &edge_count, idx, (y - 1) * width + (x + 1));
            /*верхний-левый*/
            if(x - 1 >= 0 && y - 1 >= 0) add_edge(gray, edges, &edge_count, idx, (y - 1) * width + (x - 1));
        }
    }

    /*сортируем рёбра*/
    qsort(edges, edge_count, sizeof(Edge), compare_edges);

    /*слияние*/
    for(int i = 0; i < edge_count; i++){
        float ad_threshold = threshold * (5.0f - 0.1f * (i / (float)edge_count));/*чтобы предотвращать слияние сильно отличающихся сегментов*/
        if(edges[i].weight > ad_threshold) continue;

        int root1 = uf_find(uf, edges[i].from);
        int root2 = uf_find(uf, edges[i].to);

        if(root1 != root2 && (uf->size[root1] + uf->size[root2] < 500)) uf_union(uf, root1, root2);/*объединение происходит, только если размер нового сегмента меньше 500 пикселей*/
    }

    free(edges);
}


void uf_free(UnionFind* uf) {
    if(!uf) return;
    free(uf->parent);
    free(uf->size);
    free(uf);
}


void color_30violet_blue_pink(unsigned char* output_rgb, unsigned char* gray,
                              unsigned width, unsigned height, UnionFind* uf){
    unsigned char violet[10][3] = {
        {48, 25, 52},
        {75, 0, 130},
        {138, 43, 226},
        {147, 112, 219},
        {216, 191, 216},
        {123, 104, 238},
        {153, 50, 204},
        {186, 85, 211},
        {148, 0, 211},
        {138, 43, 226}
    };

    unsigned char blue[10][3] = {
        {0, 0, 139},
        {0, 0, 205},
        {65, 105, 225},
        {70, 130, 180},
        {100, 149, 237},
        {135, 206, 235},
        {0, 191, 255},
        {30, 144, 255},
        {25, 25, 112},
        {0, 0, 255}
    };

    unsigned char pink[10][3] = {
        {255, 192, 203},
        {255, 105, 180},
        {219, 112, 147},
        {255, 20, 147},
        {199, 21, 133},
        {255, 182, 193},
        {238, 130, 238},
        {221, 160, 221},
        {255, 160, 122},
        {255, 105, 180}
    };

    for(unsigned i = 0; i < width * height; i++){
        if(i == width * height - 4) break;

        int root = uf_find(uf, i);
        int color_idx = root % 30;

        if(gray[i] == 0){
            /*шумы*/
            output_rgb[i*4] = 0;
            output_rgb[i*4 + 1] = 0;
            output_rgb[i*4 + 2] = 0;
            output_rgb[i*4 + 3] = 255;
        }
        else{
            if(color_idx < 10){
                /*фиолетовый*/
                output_rgb[i*4] = violet[color_idx][0];
                output_rgb[i*4 + 1] = violet[color_idx][1];
                output_rgb[i*4 + 2] = violet[color_idx][2];
            }
            else if (color_idx < 20){
                /*синий*/
                int blue_idx = color_idx - 10;
                output_rgb[i*4] = blue[blue_idx][0];
                output_rgb[i*4 + 1] = blue[blue_idx][1];
                output_rgb[i*4 + 2] = blue[blue_idx][2];
            }
            else{
                /*розовый*/
                int pink_idx = color_idx - 20;
                output_rgb[i*4] = pink[pink_idx][0];
                output_rgb[i*4 + 1] = pink[pink_idx][1];
                output_rgb[i*4 + 2] = pink[pink_idx][2];
            }

            output_rgb[i*4 + 3] = 255;
        }
    }
}




int main(){
    const char* input_file = "pich.png";
    const char* output_file = "output_bw.png";

    unsigned width, height;
    unsigned char* output_rgba;
    unsigned char* rgb = load_image(input_file, &width, &height);
    
    unsigned char* gray = convert_to_grayscale(rgb, width, height);
    
    
    unsigned char* filtered = malloc(width * height);
    if(!filtered) printf("Ошибка выделения памяти для фильтра\n");

    contrast_stretch(gray, width, height);

    filtered = gaussian_filter(gray, width, height);
    
    /*эту конструкцию использовать для поэьапных проверок изображения в чб*/
//    output_rgba = gray_to_rgba(filtered, width, height);
//    if(output_rgba){
//        write_png(output_file, output_rgba, width, height);
//        free(output_rgba);
//    }
//    else {
//        fprintf(stderr, "Ошибка выделения памяти для RGBA изображения\n");
//    }
    
    filtered = sobel_filter(filtered, width, height);
    
    median_filter(filtered, gray, width, height);

    /*эту конструкцию использовать для поэьапных проверок изображения в чб*/
//    output_rgba = gray_to_rgba(gray, width, height);
//    if(output_rgba){
//        write_png(output_file, output_rgba, width, height);
//        free(output_rgba);
//    }
//    else {
//        fprintf(stderr, "Ошибка выделения памяти для RGBA изображения\n");
//    }
    
    UnionFind* uf = uf_create(width * height);
    if(!uf) fprintf(stderr, "Ошибка выделения памяти для UnionFind\n");

    float threshold = 10;
    kruskal(gray, width, height, threshold, uf);

    unsigned char* output_rgb = malloc(width * height * 4);
    if(!output_rgb) fprintf(stderr, "Ошибка выделения памяти для output_rgb\n");

    color_30violet_blue_pink(output_rgb, gray, width, height, uf);
    
    write_png(output_file, output_rgb, width, height);

    
    free(rgb);
    free(gray);
    free(output_rgb);
    free(uf->parent);
    free(uf->size);
    free(uf);

    return 0;
}

