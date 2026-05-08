module.exports = {
    cppStruct: "RecipeDefinition",
    dtsInterface: "RecipeDef",

    fields: [
        {
            cpp: "id",
            jsPath: "id",
            type: "string",
            required: true,
            dtsType: "RecipeId",
            doc: "Namespaced recipe id"
        },
        {
            cpp: "type",
            jsPath: "type",
            type: "string",
            required: true,
            doc: "Recipe type (crafting, smelting, etc.)"
        },
        {
            cpp: "output",
            jsPath: "output",
            type: "string",
            required: true,
            dtsType: "ItemId",
            doc: "Output item id"
        },
        {
            cpp: "count",
            jsPath: "count",
            type: "int",
            default: 1,
            doc: "Output count"
        },
        {
            cpp: "ingredients",
            jsPath: "ingredients",
            type: "custom",
            parser: "parseStringArray",
            cppType: "std::vector<std::string>",
            dtsType: "ItemId[]",
            doc: "List of ingredient item ids"
        }
    ]
}
