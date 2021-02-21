NvsParameters::NvsParameters()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    err= nvs_open("storage", NVS_READWRITE, &my_handle);
    ESP_ERROR_CHECK(err);

}

void NvsParameters::load(char* paramname, char*out)
{
    size_t required_size;
    esp_err_t err = nvs_get_str(my_handle, paramname, NULL, &required_size);
    if(err==ESP_OK)
    {
        out = malloc(required_size);
        nvs_get_str(my_handle, paramname, out, &required_size);
        ESP_LOGI(TAG, "NvsParameters::load param=%s val=%s",paramname,out);
    } else
    {
        ESP_LOGI(TAG, "NvsParameters::load param=%s not found",paramname);
    }
}

void NvsParameters::load(char* paramname, uint_8t*out)
{
    esp_err_t err = nvs_get_u8(my_handle, paramname,  out);
    if(err==ESP_OK)
    {
        ESP_LOGI(TAG, "NvsParameters::load param=%s val=%d",paramname,out);
    } else
    {
        ESP_LOGI(TAG, "NvsParameters::load param=%s not found",paramname);
    }
}