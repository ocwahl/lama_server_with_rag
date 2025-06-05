import daisyuiThemes from 'daisyui/theme/object';
import { isNumeric } from './utils/misc';

export const isDev = import.meta.env.MODE === 'development';

// constants
export const BASE_URL = new URL('.', document.baseURI).href
  .toString()
  .replace(/\/$/, '');

export interface RagConnection {
  connection_name: string; // e.g., "Main DB", "Dev Instance"
  host: string;
  port: number;
  name: string;
  user: string;
  password?: string; // Optional if some connections don't need password
  id: string; // Using a simple timestamp or UUID for unique ID
}

export const CONFIG_DEFAULT = {
  // Note: in order not to introduce breaking changes, please keep the same data type (number, string, etc) if you want to change the default value. Do not use null or undefined for default value.
  // Do not use nested objects, keep it single level. Prefix the key if you need to group them.
  apiKey: '',
  systemMessage: '',
  showTokensPerSecond: false,
  showThoughtInProgress: false,
  excludeThoughtOnReq: true,
  // make sure these default values are in sync with `common.h`
  samplers: 'edkypmxt',
  temperature: 0.8,
  dynatemp_range: 0.0,
  dynatemp_exponent: 1.0,
  top_k: 40,
  top_p: 0.95,
  min_p: 0.05,
  xtc_probability: 0.0,
  xtc_threshold: 0.1,
  typical_p: 1.0,
  repeat_last_n: 64,
  repeat_penalty: 1.0,
  presence_penalty: 0.0,
  frequency_penalty: 0.0,
  dry_multiplier: 0.0,
  dry_base: 1.75,
  dry_allowed_length: 2,
  dry_penalty_last_n: -1,
  max_tokens: -1,
  custom: '', // custom json-stringified object
  // experimental features
  pyIntepreterEnabled: false,
  useRAG: false,
  useRERANKING: false,
  num_chunks_to_retrieve: 7,
  num_max_augmentations: 3,
  // NEW: To store the currently selected connection's name
  selected_rag_connection_name: '', // Will store the 'name' of the active connection
  // NEW: Array for RAG connections -
  rag_connections: [] as RagConnection[],
};
export const CONFIG_INFO: Record<string, string> = {
  apiKey: 'Set the API Key if you are using --api-key option for the server.',
  systemMessage: 'The starting message that defines how model should behave.',
  samplers:
    'The order at which samplers are applied, in simplified way. Default is "dkypmxt": dry->top_k->typ_p->top_p->min_p->xtc->temperature',
  temperature:
    'Controls the randomness of the generated text by affecting the probability distribution of the output tokens. Higher = more random, lower = more focused.',
  dynatemp_range:
    'Addon for the temperature sampler. The added value to the range of dynamic temperature, which adjusts probabilities by entropy of tokens.',
  dynatemp_exponent:
    'Addon for the temperature sampler. Smoothes out the probability redistribution based on the most probable token.',
  top_k: 'Keeps only k top tokens.',
  top_p:
    'Limits tokens to those that together have a cumulative probability of at least p',
  min_p:
    'Limits tokens based on the minimum probability for a token to be considered, relative to the probability of the most likely token.',
  xtc_probability:
    'XTC sampler cuts out top tokens; this parameter controls the chance of cutting tokens at all. 0 disables XTC.',
  xtc_threshold:
    'XTC sampler cuts out top tokens; this parameter controls the token probability that is required to cut that token.',
  typical_p:
    'Sorts and limits tokens based on the difference between log-probability and entropy.',
  repeat_last_n: 'Last n tokens to consider for penalizing repetition',
  repeat_penalty:
    'Controls the repetition of token sequences in the generated text',
  presence_penalty:
    'Limits tokens based on whether they appear in the output or not.',
  frequency_penalty:
    'Limits tokens based on how often they appear in the output.',
  dry_multiplier:
    'DRY sampling reduces repetition in generated text even across long contexts. This parameter sets the DRY sampling multiplier.',
  dry_base:
    'DRY sampling reduces repetition in generated text even across long contexts. This parameter sets the DRY sampling base value.',
  dry_allowed_length:
    'DRY sampling reduces repetition in generated text even across long contexts. This parameter sets the allowed length for DRY sampling.',
  dry_penalty_last_n:
    'DRY sampling reduces repetition in generated text even across long contexts. This parameter sets DRY penalty for the last n tokens.',
  max_tokens: 'The maximum number of token per output.',
  selected_rag_connection_name: 'The identifier of the rag connection.',
  host: 'The host of the rag connection.',
  name: 'The name of the rag on the rag connection (there can be several rags on one connection).',
  user: 'The user login on the rag connection.',
  password: 'The user password on the rag connection.',
  custom: '', // custom json-stringified object
};

// config keys having numeric value (i.e. temperature, top_k, top_p, etc)
export const CONFIG_NUMERIC_KEYS = Object.entries(CONFIG_DEFAULT)
  .filter((e) => isNumeric(e[1]))
  .map((e) => e[0]);
// list of themes supported by daisyui
export const THEMES = ['light', 'dark']
  // make sure light & dark are always at the beginning
  .concat(
    Object.keys(daisyuiThemes).filter((t) => t !== 'light' && t !== 'dark')
  );

// Helper function to get the currently selected RAG connection
export const getSelectedRagConnection = (
  currentLocalConfig: typeof CONFIG_DEFAULT
): RagConnection => {
  // Check if a connection name is selected and it's not the "..." placeholder
  if (
    currentLocalConfig.selected_rag_connection_name &&
    currentLocalConfig.selected_rag_connection_name !== '...'
  ) {
    const foundConnection = currentLocalConfig.rag_connections.find(
      (conn) =>
        conn.connection_name === currentLocalConfig.selected_rag_connection_name
    );

    // If a connection was found, return it
    if (foundConnection) {
      return foundConnection;
    }
    // IMPORTANT: If a connection name is selected but not found in the list,
    // it likely means the list was updated/connection deleted.
    // In this case, we fall back to returning the "default" connection details
    // based on the individual rag_db_* fields, as if it were a new/unsaved connection.
  }

  // If no specific connection is selected, or if the selected one wasn't found,
  // return a RagConnection object populated directly from the rag_db_* fields
  // that are part of the main config (which should reflect the 'current'
  // unsaved/default state of the inputs).
  // Note: 'id' is not required for connection details sent to the backend.
  return {
    connection_name:
      currentLocalConfig.selected_rag_connection_name || 'Default Connection', // Use selected name if available, else a generic default
    host: 'localhost',
    port: 5432,
    name: 'klave_rag',
    user: 'postgres',
    password: 'admin',
    id: '', // Not relevant for sending connection details to backend
  };
};
