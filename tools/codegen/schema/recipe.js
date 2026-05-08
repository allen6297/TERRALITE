module.exports = {
    cppStruct: "RecipeDefinition",
    dtsInterface: "RecipeDef",

    fields: [
        {
            cpp: "id",
            jsPath: "id",
            type: "string",
            required: true,
            validator: "recipe_id",
            dtsType: "RecipeId",
            doc: "Namespaced recipe id"
        },
        {
            cpp: "type",
            jsPath: "type",
            type: "enum",
            values: ["crafting", "smelting"],
            required: true,
            dtsType: "RecipeType",
            doc: "Recipe type (crafting, smelting, etc.)"
        },
        {
            cpp: "output",
            jsPath: "output",
            type: "string",
            required: true,
            validator: "item_id",
            dtsType: "ItemId",
            doc: "Output item id"
        },
        {
            cpp: "count",
            jsPath: "count",
            type: "int",
            default: 1,
            validator: "positive_int",
            doc: "Output count"
        },
        {
            cpp: "ingredients",
            jsPath: "ingredients",
            type: "array",
            elementType: "string",
            cppType: "std::vector<std::string>",
            validator: "item_id",
            dtsType: "ItemId[]",
            doc: "List of ingredient item ids"
        }
    ]
}
