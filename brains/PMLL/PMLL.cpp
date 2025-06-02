#include <stdio.h>
#include <stdlib.h> // For malloc, free, rand, NULL
#include <stdbool.h> // For bool type
#include <string.h> // For strcpy, snprintf, etc.
#include <unistd.h> // For sleep() in the main loop simulation
#include <time.h>   // For seeding rand()
#include <math.h>   // For sqrt, expf in conceptual functions

// --- Elaborated Conceptual Data Structures ---

typedef struct {
    char graph_id[128];
    long long node_count;
    long long edge_count;
    void* pmem_root_object; // Conceptual pointer to the root in persistent memory
    // --- NEW: Conceptual location for Transformer Model Parameters within PMLL Graph ---
    // In a real system, these would be complex structures holding billions of floats,
    // organized by layer, head, weight type (Q, K, V, FF, etc.),
    // and loaded/mapped from the PMLL.
    void* transformer_model_parameters_pmem_ptr; // Points to model weights/biases in PMLL
    int num_transformer_layers;
    int model_dimension; // d_model
    int num_attention_heads;
    int feed_forward_dim; // Dimension of the inner layer of FFN
} PMLL_Graph;

typedef struct {
    const PMLL_Graph* source_graph;
    float** node_vectors; // Input embeddings [num_vectors x vector_dim]
    int num_vectors;
    int vector_dim; // Should match source_graph->model_dimension
} Vectorized_Graph;

// This struct will now represent the output after ALL Transformer layers
typedef struct {
    const Vectorized_Graph* original_vectors;
    float** final_contextual_embeddings; // Output embeddings [num_embeddings x embedding_dim]
    int num_embeddings;
    int embedding_dim; // Should match source_graph->model_dimension
} Processed_Graph; // Renamed from Transformer_Output to reflect its role

typedef struct {
    const Processed_Graph* source_processed_graph;
    int* selected_node_indices;
    int num_selected;
    float** selected_data_vectors; // Pointers to vectors within final_contextual_embeddings
} Selection;

typedef struct {
    char id[256];
    char content[1024];
} NovelTopic;

typedef struct {
    char id[256];
    char generated_text[8192];
} WriteUp;

// --- Conceptual Transformer Sub-Component Parameters (loaded from PMLL_Graph->transformer_model_parameters_pmem_ptr) ---
// These are illustrative; a real implementation would have more complex ways to access specific weights.
typedef struct {
    // Pointers to specific weight/bias matrices for ONE layer, ONE head, or ONE FFN part
    // These would be derived from PMLL_Graph->transformer_model_parameters_pmem_ptr
    const float* Wq; // Query weights
    const float* Wk; // Key weights
    const float* Wv; // Value weights
    const float* Wo; // Output projection weights (for attention)
    const float* W_ff1; // Feed-forward layer 1 weights
    const float* b_ff1; // Feed-forward layer 1 biases
    const float* W_ff2; // Feed-forward layer 2 weights
    const float* b_ff2; // Feed-forward layer 2 biases
    const float* norm1_gamma; // LayerNorm 1 scale
    const float* norm1_beta;  // LayerNorm 1 shift
    const float* norm2_gamma; // LayerNorm 2 scale
    const float* norm2_beta;  // LayerNorm 2 shift
    // Dimensions needed for these specific matrices/vectors
    int d_model; // model dimension
    int d_k;     // dimension of key/query vectors (d_model / num_heads)
    int d_v;     // dimension of value vectors (d_model / num_heads)
    int d_ff;    // inner feed-forward dimension
} TransformerLayerComponentParams;


// --- Forward Declarations for Conceptual Transformer Sub-Components ---
void multi_head_self_attention(
    float** input_embeddings,          // [seq_len x d_model]
    float** output_embeddings,         // [seq_len x d_model] (to be filled)
    const PMLL_Graph* graph_config,    // For model_dimension, num_heads
    const TransformerLayerComponentParams* params, // Specific weights for this attention block
    int seq_len
);

void add_and_norm(
    float** input_embeddings1,         // [seq_len x d_model] (e.g., original input to layer)
    float** input_embeddings2,         // [seq_len x d_model] (e.g., output of attention/FFN)
    float** output_embeddings,         // [seq_len x d_model] (to be filled)
    const float* gamma, const float* beta, // LayerNorm params
    int seq_len, int d_model
);

void positionwise_feed_forward(
    float** input_embeddings,          // [seq_len x d_model]
    float** output_embeddings,         // [seq_len x d_model] (to be filled)
    const TransformerLayerComponentParams* params, // Specific weights/biases for FFN
    int seq_len
);


// --- Elaborated Placeholder Function Declarations (Stubs) ---

PMLL_Graph* pmll_load_or_initialize_graph_elaborated(const char* graph_name) {
    printf("[PMLL] Loading or initializing persistent graph: %s...\n", graph_name);
    PMLL_Graph* graph = (PMLL_Graph*)malloc(sizeof(PMLL_Graph));
    if (!graph) {
        perror("Failed to allocate PMLL_Graph structure");
        return NULL;
    }
    strncpy(graph->graph_id, graph_name, sizeof(graph->graph_id) - 1);
    graph->graph_id[sizeof(graph->graph_id) - 1] = '\0';
    graph->node_count = 1000;
    graph->edge_count = 5000;
    graph->pmem_root_object = NULL; 
    
    // --- Initialize conceptual Transformer parameters ---
    graph->transformer_model_parameters_pmem_ptr = NULL; // Conceptual; real would point to PMEM
    graph->num_transformer_layers = 6;  // Example: A small Transformer
    graph->model_dimension = 128;       // d_model
    graph->num_attention_heads = 4;     // num_heads
    graph->feed_forward_dim = graph->model_dimension * 4; // Common practice: d_ff = 4 * d_model

    printf("[PMLL] Graph '%s' initialized. Nodes: %lld, Edges: %lld\n",
           graph->graph_id, graph->node_count, graph->edge_count);
    printf("[PMLL] Conceptual Transformer Config: Layers: %d, Dim: %d, Heads: %d, FF_Dim: %d\n",
           graph->num_transformer_layers, graph->model_dimension, graph->num_attention_heads, graph->feed_forward_dim);
    return graph;
}

Vectorized_Graph* vectorize_from_pmll_elaborated(const PMLL_Graph* p_graph) {
    if (!p_graph) return NULL;
    printf("[VECTORIZE] Vectorizing data from PMLL graph '%s'...\n", p_graph->graph_id);

    Vectorized_Graph* v_graph = (Vectorized_Graph*)malloc(sizeof(Vectorized_Graph));
    if (!v_graph) {
        perror("Failed to allocate Vectorized_Graph structure");
        return NULL;
    }
    v_graph->source_graph = p_graph;
    v_graph->num_vectors = p_graph->node_count; 
    v_graph->vector_dim = p_graph->model_dimension; // Embeddings match model dimension

    v_graph->node_vectors = (float**)malloc(v_graph->num_vectors * sizeof(float*));
    if (!v_graph->node_vectors) {
        perror("Failed to allocate node_vectors array of pointers");
        free(v_graph);
        return NULL;
    }
    // Simulate allocating and initializing dummy embedding vectors
    // In a real system, these would be loaded from PMLL or computed based on graph content
    for (int i = 0; i < v_graph->num_vectors; ++i) {
        v_graph->node_vectors[i] = (float*)malloc(v_graph->vector_dim * sizeof(float));
        if (!v_graph->node_vectors[i]) {
            perror("Failed to allocate individual node vector");
            // Cleanup previously allocated vectors
            for (int k = 0; k < i; ++k) free(v_graph->node_vectors[k]);
            free(v_graph->node_vectors);
            free(v_graph);
            return NULL;
        }
        // Initialize with some dummy values
        for (int j = 0; j < v_graph->vector_dim; ++j) {
            v_graph->node_vectors[i][j] = (float)rand() / RAND_MAX * 0.1f; // Small random values
        }
    }
    printf("[VECTORIZE] Conceptual vectorization complete. Num vectors: %d, Dim: %d\n",
           v_graph->num_vectors, v_graph->vector_dim);
    return v_graph;
}

// --- Transformer Core Logic (Elaborated Stubs) ---

Processed_Graph* process_with_transformer_layers_elaborated(const Vectorized_Graph* v_graph, const PMLL_Graph* graph_config) {
    if (!v_graph || !graph_config) return NULL;
    printf("[TRANSFORMER_CORE] Processing %d vectors of dim %d through %d layers...\n",
           v_graph->num_vectors, v_graph->vector_dim, graph_config->num_transformer_layers);

    if (v_graph->vector_dim != graph_config->model_dimension) {
        fprintf(stderr, "[TRANSFORMER_CORE] Error: Vector dimension (%d) does not match model dimension (%d).\n",
                v_graph->vector_dim, graph_config->model_dimension);
        return NULL;
    }

    Processed_Graph* proc_graph = (Processed_Graph*)malloc(sizeof(Processed_Graph));
    if (!proc_graph) {
        perror("Failed to allocate Processed_Graph structure");
        return NULL;
    }
    proc_graph->original_vectors = v_graph;
    proc_graph->num_embeddings = v_graph->num_vectors;
    proc_graph->embedding_dim = v_graph->vector_dim;

    // Allocate memory for the final output embeddings
    proc_graph->final_contextual_embeddings = (float**)malloc(proc_graph->num_embeddings * sizeof(float*));
    if (!proc_graph->final_contextual_embeddings) {
        perror("Failed to allocate final_contextual_embeddings array of pointers");
        free(proc_graph);
        return NULL;
    }
    // Allocate memory for each embedding vector
    for (int i = 0; i < proc_graph->num_embeddings; ++i) {
        proc_graph->final_contextual_embeddings[i] = (float*)malloc(proc_graph->embedding_dim * sizeof(float));
        if (!proc_graph->final_contextual_embeddings[i]) {
            perror("Failed to allocate individual final embedding vector");
            for (int k = 0; k < i; ++k) free(proc_graph->final_contextual_embeddings[k]);
            free(proc_graph->final_contextual_embeddings);
            free(proc_graph);
            return NULL;
        }
        // Initialize/copy from input: current_x starts as input embeddings
        memcpy(proc_graph->final_contextual_embeddings[i], v_graph->node_vectors[i], proc_graph->embedding_dim * sizeof(float));
    }

    // Temporary buffer for intermediate layer outputs (e.g., after attention, before add&norm)
    // In a highly optimized version, this might be managed more carefully (in-place ops, etc.)
    float** temp_embeddings = (float**)malloc(proc_graph->num_embeddings * sizeof(float*));
    if (!temp_embeddings) { /* ... error handling ... */ return NULL; }
    for (int i = 0; i < proc_graph->num_embeddings; ++i) {
        temp_embeddings[i] = (float*)malloc(proc_graph->embedding_dim * sizeof(float));
        if (!temp_embeddings[i]) { /* ... error handling ... */ return NULL; }
    }

    // --- Loop through each Transformer Layer ---
    for (int layer_idx = 0; layer_idx < graph_config->num_transformer_layers; ++layer_idx) {
        printf("  [Layer %d/%d]\n", layer_idx + 1, graph_config->num_transformer_layers);

        // Conceptual: Load/get pointers to parameters for THIS layer from PMLL
        // This is highly simplified. A real system would map specific memory regions
        // from graph_config->transformer_model_parameters_pmem_ptr based on layer_idx.
        TransformerLayerComponentParams current_layer_params;
        current_layer_params.d_model = graph_config->model_dimension;
        current_layer_params.d_k = graph_config->model_dimension / graph_config->num_attention_heads;
        current_layer_params.d_v = graph_config->model_dimension / graph_config->num_attention_heads;
        current_layer_params.d_ff = graph_config->feed_forward_dim;
        // Wq, Wk, Wv etc. would be pointers to actual float arrays from PMLL for this layer.
        // For this stub, we'll leave them as NULL or point to dummy static data if needed for deeper stubs.
        current_layer_params.Wq = NULL; /* ... and so on for other weights/biases */
        current_layer_params.norm1_gamma = NULL; /* ... etc ... */


        // 1. Multi-Head Self-Attention
        // Input: proc_graph->final_contextual_embeddings (output of previous layer, or initial embeddings)
        // Output: temp_embeddings (output of attention mechanism for this layer)
        printf("    - Multi-Head Self-Attention...\n");
        multi_head_self_attention(proc_graph->final_contextual_embeddings, temp_embeddings,
                                  graph_config, &current_layer_params, proc_graph->num_embeddings);

        // 2. Add & Norm (Residual connection + Layer Normalization)
        // Input1: proc_graph->final_contextual_embeddings (input to the attention sublayer, i.e., x)
        // Input2: temp_embeddings (output of attention sublayer, i.e., Attention(x))
        // Output: proc_graph->final_contextual_embeddings (overwriting with SublayerOutput(x + Attention(x)))
        printf("    - Add & Norm 1...\n");
        add_and_norm(proc_graph->final_contextual_embeddings, temp_embeddings,
                     proc_graph->final_contextual_embeddings, // Output overwrites previous state
                     current_layer_params.norm1_gamma, current_layer_params.norm1_beta,
                     proc_graph->num_embeddings, proc_graph->embedding_dim);

        // Store the output of Add & Norm 1 to be the input for the FFN's Add & Norm
        // This requires another buffer or careful handling if we want to keep the pre-FFN state for the residual.
        // For simplicity, let's assume temp_embeddings can be reused or we operate on copies.
        // Or, more accurately, the input to FFN is the output of the first Add & Norm.
        // Let's make a copy for the residual connection around FFN.
        float** ffn_input_for_residual = (float**)malloc(proc_graph->num_embeddings * sizeof(float*));
        // ... (allocate and copy proc_graph->final_contextual_embeddings to ffn_input_for_residual) ...
        for(int i=0; i<proc_graph->num_embeddings; ++i) {
            ffn_input_for_residual[i] = (float*)malloc(proc_graph->embedding_dim * sizeof(float));
            memcpy(ffn_input_for_residual[i], proc_graph->final_contextual_embeddings[i], proc_graph->embedding_dim * sizeof(float));
        }


        // 3. Position-wise Feed-Forward Network
        // Input: proc_graph->final_contextual_embeddings (output of first Add & Norm)
        // Output: temp_embeddings (output of FFN for this layer)
        printf("    - Position-wise Feed-Forward Network...\n");
        positionwise_feed_forward(proc_graph->final_contextual_embeddings, temp_embeddings,
                                  &current_layer_params, proc_graph->num_embeddings);
        
        // 4. Add & Norm (Residual connection + Layer Normalization)
        // Input1: ffn_input_for_residual (output of first Add & Norm, i.e. input to FFN sublayer)
        // Input2: temp_embeddings (output of FFN sublayer)
        // Output: proc_graph->final_contextual_embeddings (final output for this Transformer layer)
        printf("    - Add & Norm 2...\n");
        add_and_norm(ffn_input_for_residual, temp_embeddings,
                     proc_graph->final_contextual_embeddings, // Output overwrites
                     current_layer_params.norm2_gamma, current_layer_params.norm2_beta,
                     proc_graph->num_embeddings, proc_graph->embedding_dim);

        // Free the temporary copy for FFN residual
        for(int i=0; i<proc_graph->num_embeddings; ++i) free(ffn_input_for_residual[i]);
        free(ffn_input_for_residual);
    }

    // Free temporary buffer
    for (int i = 0; i < proc_graph->num_embeddings; ++i) free(temp_embeddings[i]);
    free(temp_embeddings);

    printf("[TRANSFORMER_CORE] All %d layers processed. Final contextual embeddings generated.\n", graph_config->num_transformer_layers);
    return proc_graph;
}


// --- Stubs for Transformer Sub-Components ---
void multi_head_self_attention(
    float** input_embeddings, float** output_embeddings,
    const PMLL_Graph* graph_config, const TransformerLayerComponentParams* params,
    int seq_len) {
    // This is a major simplification. Real MHA involves:
    // For each head:
    // 1. Linear projections of input_embeddings to Q, K, V matrices using params->Wq, Wk, Wv.
    //    (seq_len x d_model) -> (seq_len x d_k or d_v)
    // 2. Scaled Dot-Product Attention:
    //    AttentionScores = softmax((Q * K^T) / sqrt(d_k))
    //    AttendedValues = AttentionScores * V
    //    This involves multiple matrix multiplications.
    // Concatenate outputs of all heads: (seq_len x (num_heads * d_v)) -> (seq_len x d_model if num_heads*d_v = d_model)
    // Final linear projection using params->Wo.

    printf("      (Stub) Performing Multi-Head Self-Attention for %d tokens. Heads: %d, Dim: %d...\n",
           seq_len, graph_config->num_attention_heads, graph_config->model_dimension);
    // For stub: just copy input to output (no actual attention)
    for (int i = 0; i < seq_len; ++i) {
        memcpy(output_embeddings[i], input_embeddings[i], graph_config->model_dimension * sizeof(float));
        // Simulate some processing by slightly altering values
        if (graph_config->model_dimension > 0) output_embeddings[i][0] += 0.01f; 
    }
}

void add_and_norm(
    float** input_embeddings1, float** input_embeddings2, float** output_embeddings,
    const float* gamma, const float* beta, // LayerNorm params
    int seq_len, int d_model) {
    // 1. Add: output_temp[i][j] = input_embeddings1[i][j] + input_embeddings2[i][j]
    // 2. Layer Normalization on output_temp:
    //    For each embedding vector in output_temp:
    //      Calculate mean and variance across its d_model dimensions.
    //      Normalize: (x - mean) / sqrt(variance + epsilon)
    //      Scale and shift: normalized_x * gamma + beta
    //    Store result in output_embeddings.
    //    (gamma and beta are learnable parameters, d_model dimensional)

    printf("      (Stub) Performing Add & Layer Normalization for %d tokens, dim %d...\n", seq_len, d_model);
    float epsilon = 1e-5f; // Small value to prevent division by zero

    for (int i = 0; i < seq_len; ++i) {
        // Add
        for (int j = 0; j < d_model; ++j) {
            output_embeddings[i][j] = input_embeddings1[i][j] + input_embeddings2[i][j];
        }

        // LayerNorm (conceptual, on the sum)
        float mean = 0.0f;
        for (int j = 0; j < d_model; ++j) mean += output_embeddings[i][j];
        mean /= d_model;

        float variance = 0.0f;
        for (int j = 0; j < d_model; ++j) variance += powf(output_embeddings[i][j] - mean, 2);
        variance /= d_model;

        for (int j = 0; j < d_model; ++j) {
            float normalized_x = (output_embeddings[i][j] - mean) / sqrtf(variance + epsilon);
            // Apply scale (gamma) and shift (beta) - if params were provided
            // For stub, if gamma/beta are NULL, assume gamma=1, beta=0
            float current_gamma = (gamma && gamma[j]) ? gamma[j] : 1.0f;
            float current_beta  = (beta && beta[j]) ? beta[j] : 0.0f;
            output_embeddings[i][j] = normalized_x * current_gamma + current_beta;
        }
    }
}

void positionwise_feed_forward(
    float** input_embeddings, float** output_embeddings,
    const TransformerLayerComponentParams* params, int seq_len) {
    // For each position (independently):
    // 1. Linear transformation: hidden = activation(input_embeddings * W_ff1 + b_ff1)
    //    (d_model -> d_ff)
    //    Activation is often ReLU or GeLU.
    // 2. Linear transformation: output_embeddings = hidden * W_ff2 + b_ff2
    //    (d_ff -> d_model)

    printf("      (Stub) Performing Position-wise Feed-Forward Network for %d tokens (d_model: %d, d_ff: %d)...\n",
           seq_len, params->d_model, params->d_ff);
    // For stub: just copy input to output (no actual FFN)
    for (int i = 0; i < seq_len; ++i) {
        memcpy(output_embeddings[i], input_embeddings[i], params->d_model * sizeof(float));
        // Simulate some processing
        if (params->d_model > 0) output_embeddings[i][0] -= 0.005f;
    }
}


Selection* select_relevant_from_graph_elaborated(const Processed_Graph* p_graph, const NovelTopic* topic) {
    if (!p_graph || !topic) return NULL;
    printf("[SELECT] Selecting relevant data from processed graph for topic: %s...\n", topic->id);

    Selection* selection = (Selection*)malloc(sizeof(Selection));
    if (!selection) {
        perror("Failed to allocate Selection structure");
        return NULL;
    }
    selection->source_processed_graph = p_graph;
    // Select a small, random number of embeddings for demonstration
    selection->num_selected = (p_graph->num_embeddings > 0) ? (rand() % (p_graph->num_embeddings / 20 + 1)) + 1 : 0;
    if (selection->num_selected == 0 && p_graph->num_embeddings > 0) selection->num_selected = 1; // Ensure at least one if possible

    if (selection->num_selected > 0) {
        selection->selected_node_indices = (int*)malloc(selection->num_selected * sizeof(int));
        selection->selected_data_vectors = (float**)malloc(selection->num_selected * sizeof(float*)); // Array of pointers

        if (!selection->selected_node_indices || !selection->selected_data_vectors) {
            perror("Failed to allocate selection arrays");
            if (selection->selected_node_indices) free(selection->selected_node_indices);
            if (selection->selected_data_vectors) free(selection->selected_data_vectors);
            free(selection);
            return NULL;
        }

        for (int i = 0; i < selection->num_selected; ++i) {
            selection->selected_node_indices[i] = rand() % p_graph->num_embeddings;
            // Point to the actual (conceptually final) embedding data
            selection->selected_data_vectors[i] = p_graph->final_contextual_embeddings[selection->selected_node_indices[i]];
        }
    } else {
        selection->selected_node_indices = NULL;
        selection->selected_data_vectors = NULL;
    }
    printf("[SELECT] Selected %d relevant items (conceptually).\n", selection->num_selected);
    return selection;
}

WriteUp* rewrite_or_generate_write_up_elaborated(const Selection* selection, const NovelTopic* topic) {
    if (!selection || !topic) return NULL;
    printf("[REWRITE] Generating/rewriting write-up for topic: %s based on selection (num_selected: %d)...\n",
           topic->id, selection->num_selected);

    WriteUp* new_write_up = (WriteUp*)malloc(sizeof(WriteUp));
    if (!new_write_up) {
        perror("Failed to allocate WriteUp structure");
        return NULL;
    }

    snprintf(new_write_up->id, sizeof(new_write_up->id), "writeup_for_%s", topic->id);
    if (selection->num_selected > 0 && selection->selected_node_indices != NULL) {
        snprintf(new_write_up->generated_text, sizeof(new_write_up->generated_text),
                 "This is an ELABORATED TRANSFORMED write-up for the novel topic '%s'. "
                 "Content derived from a selection of %d items from the PMLL graph after %d Transformer layers. "
                 "First selected item index (conceptual): %d. Data (conceptual first float): %f",
                 topic->content, selection->num_selected, 
                 selection->source_processed_graph->original_vectors->source_graph->num_transformer_layers,
                 selection->selected_node_indices[0],
                 (selection->selected_data_vectors && selection->selected_data_vectors[0] && selection->source_processed_graph->embedding_dim > 0) ? selection->selected_data_vectors[0][0] : 0.0f);
    } else {
        snprintf(new_write_up->generated_text, sizeof(new_write_up->generated_text),
                 "This is an ELABORATED TRANSFORMED write-up for the novel topic '%s'. "
                 "No specific items were selected from the processed PMLL graph for this topic.",
                 topic->content);
    }
    printf("[REWRITE] Write-up generation complete.\n");
    return new_write_up;
}

NovelTopic* get_next_novel_topic(int counter) {
    printf("\n[SYSTEM] Checking for novel topics...\n");
    NovelTopic* topic = (NovelTopic*)malloc(sizeof(NovelTopic));
    if (topic) {
        snprintf(topic->id, sizeof(topic->id), "topic_%d", counter);
        snprintf(topic->content, sizeof(topic->content), "Transformed Novel Topic %d: Implications of Multi-Layered Contextual Embeddings from PMLL.", counter);
        printf("[SYSTEM] New novel topic received: %s - '%s'\n", topic->id, topic->content);
    }
    return topic;
}

void print_generated_write_up(const WriteUp* write_up) {
    if (!write_up) return;
    printf("\n--- Generated Write-Up (ID: %s) ---\n", write_up->id);
    printf("%s\n", write_up->generated_text);
    printf("--- End of Write-Up ---\n");
}

void free_pmll_graph_elaborated(PMLL_Graph* graph) {
    if (!graph) return;
    printf("[PMLL] Freeing conceptual PMLL_Graph structure for '%s'.\n", graph->graph_id);
    free(graph);
}

void free_vectorized_graph_elaborated(Vectorized_Graph* v_graph) {
    if (!v_graph) return;
    printf("[VECTORIZE] Freeing Vectorized_Graph structure and its dummy vectors.\n");
    if (v_graph->node_vectors) {
        for (int i = 0; i < v_graph->num_vectors; ++i) {
            if (v_graph->node_vectors[i]) free(v_graph->node_vectors[i]);
        }
        free(v_graph->node_vectors);
    }
    free(v_graph);
}

void free_processed_graph_elaborated(Processed_Graph* p_graph) {
    if (!p_graph) return;
    printf("[TRANSFORMER_CORE] Freeing Processed_Graph structure and its final embeddings.\n");
    if (p_graph->final_contextual_embeddings) {
        for (int i = 0; i < p_graph->num_embeddings; ++i) {
            if (p_graph->final_contextual_embeddings[i]) free(p_graph->final_contextual_embeddings[i]);
        }
        free(p_graph->final_contextual_embeddings);
    }
    free(p_graph);
}

void free_selection_elaborated(Selection* selection) {
    if (!selection) return;
    printf("[SELECT] Freeing Selection structure.\n");
    if (selection->selected_node_indices) free(selection->selected_node_indices);
    // selected_data_vectors points to data within Processed_Graph, so not freed separately here
    // to avoid double free, only the array of pointers itself.
    if (selection->selected_data_vectors) free(selection->selected_data_vectors);
    free(selection);
}

void free_novel_topic(NovelTopic* topic) {
    if (topic) free(topic);
}

void free_write_up(WriteUp* write_up) {
    if (write_up) free(write_up);
}


// --- Main Program Loop ---
int main() {
    srand(time(NULL));

    printf("Initializing ELABORATED & TRANSFORMER-DETAILED Conceptual PMLL Processing System...\n");

    PMLL_Graph* main_pmll_graph = pmll_load_or_initialize_graph_elaborated("my_knowledge_base.pmll");
    if (!main_pmll_graph) {
        fprintf(stderr, "Fatal: Could not initialize PMLL graph. Exiting.\n");
        return 1;
    }

    int topic_counter = 0;
    printf("\nStarting main processing loop (Ctrl+C to exit for infinite loop, or runs 3 iterations)...\n");

    // while (true) { // Or use a counter for demo
    while (topic_counter < 2) { // Run for a few iterations for demonstration
        NovelTopic* current_topic = get_next_novel_topic(++topic_counter);
        if (!current_topic) {
            printf("[SYSTEM] No new topic, sleeping...\n");
            sleep(1);
            continue;
        }

        Vectorized_Graph* vectorized_data = vectorize_from_pmll_elaborated(main_pmll_graph);
        if (!vectorized_data) {
            fprintf(stderr, "[ERROR] Failed to vectorize PMLL graph for topic %s.\n", current_topic->id);
            free_novel_topic(current_topic);
            continue;
        }

        // Pass main_pmll_graph to get transformer config like num_layers, d_model etc.
        Processed_Graph* processed_data = process_with_transformer_layers_elaborated(vectorized_data, main_pmll_graph);
        // Vectorized_data's content (node_vectors) is conceptually used as the initial input
        // to the transformer layers. The Processed_Graph will contain the final output.
        // We free vectorized_data wrapper and its allocated dummy vectors after it's used.
        free_vectorized_graph_elaborated(vectorized_data); 
        
        if (!processed_data) {
            fprintf(stderr, "[ERROR] Failed to process graph with transformer layers for topic %s.\n", current_topic->id);
            free_novel_topic(current_topic);
            continue;
        }

        Selection* selection = select_relevant_from_graph_elaborated(processed_data, current_topic);
        // Processed_data's content (final_contextual_embeddings) is used by selection.
        // We free processed_data wrapper and its allocated dummy embeddings after.
        free_processed_graph_elaborated(processed_data); 
        
        if (!selection) {
            fprintf(stderr, "[ERROR] Failed to select relevant data for topic %s.\n", current_topic->id);
            free_novel_topic(current_topic);
            continue;
        }

        WriteUp* final_write_up = rewrite_or_generate_write_up_elaborated(selection, current_topic);
        free_selection_elaborated(selection);

        if (final_write_up) {
            print_generated_write_up(final_write_up);
            free_write_up(final_write_up);
        } else {
            fprintf(stderr, "[ERROR] Failed to generate write-up for topic %s.\n", current_topic->id);
        }
        
        free_novel_topic(current_topic);

        printf("\n[SYSTEM] Topic processing complete. Waiting for next cycle...\n");
        sleep(1);
    }

    printf("\n[SYSTEM] Demo loop finished.\n");
    printf("[PMLL] System shutting down. Persisting final graph state (conceptual)...\n");
    free_pmll_graph_elaborated(main_pmll_graph);

    return 0;
}
